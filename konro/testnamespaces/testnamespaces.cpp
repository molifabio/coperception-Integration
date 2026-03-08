#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "namespaces.h"

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstdint>
#include <cstring>

#include <unistd.h>
#include <sched.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <linux/nsfs.h>

int pipefd[2];
static char child_stack[1048576];

static void get_namespace(const char *msg, int fd, long request)
{
    printf("%s\n", msg);

    struct stat sb;
    int nsfd;

    if (fstat(fd, &sb) == -1) {
        perror("fstat-fd");
        exit(EXIT_FAILURE);
    }

    printf("Namespace: [%jx,%jx] / %ju\n",
           (uintmax_t) major(sb.st_dev),
           (uintmax_t) minor(sb.st_dev),
           (uintmax_t) sb.st_ino);

    nsfd = ioctl(fd, NS_GET_NSTYPE);
    if (nsfd == -1) {
        perror("NS_GET_NSTYPE");
        return;
    }
    printf("%s: NS_GET_NSTYPE: 0x%X\n", msg, nsfd);

    nsfd = ioctl(fd, request);
    if (nsfd == -1) {
        perror(msg);
        return;
    }

    if (fstat(nsfd, &sb) == -1) {
        perror("fstat-userns");
        exit(EXIT_FAILURE);
    }

    printf("%s: [%jx,%jx] / %ju\n",
           msg,
           (uintmax_t) major(sb.st_dev),
           (uintmax_t) minor(sb.st_dev),
           (uintmax_t) sb.st_ino);
    close(nsfd);

    nsfd = ioctl(nsfd, NS_GET_NSTYPE);
    if (nsfd == -1) {
        perror("NS_GET_NSTYPE");
        return;
    }
    printf("%s: nsfd NS_GET_NSTYPE: 0x%X\n\n\n", msg, nsfd);
    close(nsfd);
}

static int child_fn(void *)
{
    char fname[256];

    printf("Child PID: %ld\n", (long)getpid());

    //sprintf(fname, "/proc/%ld/ns/pid", (long)getpid());
    strcpy(fname, "/proc/self/ns/pid");
    int fd = open(fname, O_RDONLY);
    if (fd == -1) {
        char buf[320];
        sprintf(buf, "open %s", fname);
        perror(buf);
        return EXIT_FAILURE;
    }

    // Obtain a file descriptor for the owning user namespace

    get_namespace("Owning user namespace", fd, NS_GET_USERNS);
    get_namespace("Parent user namespace", fd, NS_GET_PARENT);

    close(fd);

    //close(pipefd[0]);   // close read end
    long mypid = (long)getpid();
    unsigned long myns = rmcommon::getSelfPidNamespace();
    int nr;
    nr = write(pipefd[1], &mypid, sizeof(mypid));
    if (nr != sizeof(mypid)) {
        perror("write");
    }
    nr = write(pipefd[1], &myns, sizeof(myns));
    if (nr != sizeof(myns)) {
        perror("write");
    }

    printf("Child: pid=%ld ns=%lu\n", mypid, myns);
    sleep(10);
    return EXIT_SUCCESS;
}


int main(int argc, char *argv[])
{
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    //close(pipefd[1]);   // close write end

    // To create a new PID namespace, one must call the clone() system
    // call with a special flag CLONE_NEWPID

    pid_t child_pid = clone(child_fn,
                            child_stack+sizeof(child_stack),
                            CLONE_NEWPID | CLONE_FILES | SIGCHLD,
                            NULL);
    printf("Main PID: %ld\n", (long)getpid());
    printf("clone() = %ld\n", (long)child_pid);

    long childpid;
    unsigned long childns;
    int nr;

    nr = read(pipefd[0], &childpid, sizeof(childpid));
    if (nr == -1) {
        perror("read");
    } else{
        printf("Main: %d bytes read\n", nr);
    }
    nr = read(pipefd[0], &childns, sizeof(childns));
    if (nr == -1) {
        perror("read");
    } else{
        printf("Main: %d bytes read\n", nr);
    }

    printf("Main: child pid=%ld, child ns=%lu\n", childpid, childns);

    long mapped_pid = rmcommon::mapPid(childpid, childns);
    printf("Main: mapped pid=%ld\n", mapped_pid);
    waitpid(child_pid, NULL, 0);

    return EXIT_SUCCESS;
}
