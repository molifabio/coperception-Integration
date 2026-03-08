#include "namespaces.h"
//#include "../platformcontrol/cgroup/cgrouputil.h"
#include "dir.h"
#include <sstream>
#include <iostream>
#include <string>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace rmcommon {

namespace {

    /*!
     * Parses the status file, searching for the line
     * that begins with "NSpid:"
     * \p
     * The first pid of the line is the PID in the current
     * namespace; the last PID on the line is the pid in the
     * original namespace
     *
     * \param filename Pathname of the "status" file
     * \param pidToSearch [in/out]
     * \return Outcome of the operation
     */
    bool getPidFromStatusFile(const char *filename, pid_t &pidToSearch) {

        std::cout << "Parsing " << filename << std::endl;

        std::ifstream ifs;
        ifs.open(filename);
        if (!ifs.is_open())
            return false;
        char line[1024];
        pid_t rootpid = -1, last_pid;
        while (ifs.getline(line, sizeof(line))) {
            if (strncmp(line, "NSpid:", 6) == 0) {
                std::cout << "NSpid line found:";
                char *tok = line + 6;
                while ((tok = strtok(tok, " \t")) != nullptr) {
                    last_pid = strtol(tok, nullptr, 10);
                    std::cout << " " << last_pid;
                    if (rootpid == -1) {
                        rootpid = last_pid;  // rootpid: first pid on the line
                    }
                    tok = nullptr;
                }
                std::cout << std::endl;
                break;
            }
        }
        // the search is successful if the last pid on the line is the
        // pid that we are looking for
        if (pidToSearch == last_pid) {
            pidToSearch = rootpid;
            return true;
        } else {
            return false;
        }
    }
}

bool isKubPod(pid_t pid)
{
    std::ostringstream os;
    os << "/proc/" << pid << "/cgroup";
    std::ifstream in(os.str());
    if (!in.is_open())
        perror("open");
    std::string cgroupPath;
    // for example: "0::/init.scope"
    if (!(in >> cgroupPath))
        perror("read");
    return cgroupPath.find("kubepods") != std::string::npos;
}

#define PID_FNAME "/proc/self/ns/pid"

namespace_t getSelfPidNamespace()
{
    namespace_t rc = 0;

    int fd = open(PID_FNAME, O_RDONLY);
    if (fd == -1) {
        perror("open " PID_FNAME);
    } else {
        struct stat sb;
        if (fstat(fd, &sb) == -1) {
            perror("fstat fd of " PID_FNAME);
        } else {
            rc = sb.st_ino;
        }
        close(fd);
    }
    return rc;
}

namespace_t getPidNamespace(pid_t pid)
{
    char filename[512];
    namespace_t ns = 0;

    snprintf(filename, sizeof(filename), "/proc/%ld/ns/pid",
             static_cast<long>(pid));

    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("open");
    } else {
        struct stat sb;
        if (fstat(fd, &sb) == -1) {
            perror("fstat");
        } else {
            ns = sb.st_ino;
        }
        close(fd);
    }
    return ns;
}

pid_t mapPid(pid_t pid, namespace_t ns)
{
    /* The ns is invalid or is the same as the Konro's one */
    /* In both cases the PID stays the same */
    if (ns == 0 || ns == getSelfPidNamespace())
        return pid;

    Dir dir = Dir::localdir("/proc");

    /* The only way to access the namespace fs is through
       the proc pseudo-fs */
    for (const auto &entry: dir) {
        const std::string &name = entry.name();
        if (name.empty() || !isdigit(name[0])) {
            continue;
        }
        pid_t curPid = (pid_t)strtoul(name.c_str(), nullptr, 10);
        namespace_t curNs = getPidNamespace(curPid);
        if (curNs == ns) {
            // the process belongs to the searched namespace
            std::cout << "Namespace of " << curPid << " is " << curNs << std::endl;
            char filename[64];
            snprintf(filename, sizeof(filename), "/proc/%ld/status",
                     static_cast<long>(curPid));
            if (getPidFromStatusFile(filename, pid)) {
                break;
            }
        }
    }
    return pid;
}

#undef PID_FNAME

}   // namespace rmcommon
