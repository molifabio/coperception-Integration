#include "proclistener.h"
#include "forkevent.h"
#include "execevent.h"
#include "exitevent.h"
#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/connector.h>
#include <linux/cn_proc.h>

// fix missing gettid() function in glibc
#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 30
#include <sys/syscall.h>
#define gettid() syscall(SYS_gettid)
#endif

using namespace std;

namespace wm {

#define KERNEL_PID                  0
#define NLMSG_STOP_MESSAGE_TYPE     (NLMSG_MIN_TYPE+12345)


ProcListener::ProcListener(rmcommon::EventBus &eventBus) :
    bus_(eventBus),
    cat_(log4cpp::Category::getRoot()) {
}

int ProcListener::createNetlinkSocket()
{
    int sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
    if (sock == -1) {
        errno_ = errno;
    }
    return sock;
}

bool ProcListener::bindNetlinkSocket(int sock, unsigned int pid)
{
    sockaddr_nl addrNl;
    memset(&addrNl, 0, sizeof(addrNl));
    addrNl.nl_family = AF_NETLINK;
    addrNl.nl_groups = CN_IDX_PROC;
    addrNl.nl_pid = pid;

    int rc = bind(sock, reinterpret_cast<struct sockaddr *>(&addrNl), sizeof(addrNl));
    if (rc) {
        errno_ = errno;
        return false;
    }
    return true;
}

bool ProcListener::sendNetlinkMessage(int sock, void *buf, std::size_t bufsize, unsigned int pid, unsigned int groups)
{
    sockaddr_nl dstAddr;
    memset(&dstAddr, 0, sizeof(dstAddr));
    dstAddr.nl_family = AF_NETLINK;
    dstAddr.nl_pid = pid;
    dstAddr.nl_groups = groups;

    // scatter-gather I/O

    struct iovec iov;
    iov.iov_base = buf;
    iov.iov_len = bufsize;

    // Create and send the message

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = &dstAddr;
    msg.msg_namelen = sizeof(sockaddr_nl);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    int rc = true;
    ssize_t nsent = sendmsg(sock, &msg, 0);
    ostringstream os;
    os << "PROCLISTENER sendNetlinkMessage: nsent is " << nsent << ", nlmsgSize is " << bufsize;
    cat_.debug(os.str());

    if (nsent < 0 || (size_t)nsent != bufsize) {
        errno_ = errno;
        rc = false;
    }

    return rc;
}

bool ProcListener::sendConnectorNetlinkMessageToKernel(int socket, MessageData op)
{
    constexpr size_t dataSize = sizeof(op);
    constexpr size_t nlmsgSize = NLMSG_SPACE(sizeof(struct cn_msg) + dataSize);

    uint8_t messageBuffer[1024];
    memset(messageBuffer, 0, sizeof(messageBuffer));

    // The netlink message header is at the start of the buffer
    // and is followed by the connector message
    struct nlmsghdr *nlHdr = (struct nlmsghdr *)messageBuffer;
    struct cn_msg *cnMsg = (struct cn_msg *)(NLMSG_DATA(messageBuffer));

    nlHdr->nlmsg_len = nlmsgSize;
    nlHdr->nlmsg_type = NLMSG_DONE;
    nlHdr->nlmsg_pid = gettid();

    cnMsg->id.idx = CN_IDX_PROC;    // idx and val are used for message routing
    cnMsg->id.val = CN_VAL_PROC;
    cnMsg->len = dataSize;
    memcpy(cnMsg->data, &op, dataSize);

    return sendNetlinkMessage(socket, messageBuffer, nlmsgSize, KERNEL_PID, CN_IDX_PROC);
}

bool ProcListener::sendConnectorNetlinkMessageToThread(int socket, MessageData op)
{
    constexpr size_t dataSize = sizeof(op);
    constexpr size_t nlmsgSize = NLMSG_SPACE(sizeof(struct cn_msg) + dataSize);

    uint8_t messageBuffer[1024];
    memset(messageBuffer, 0, sizeof(messageBuffer));

    // The netlink message header is at the start of the buffer
    // and is followed by the connector message
    struct nlmsghdr *nlHdr = (struct nlmsghdr *)messageBuffer;
    struct cn_msg *cnMsg = (struct cn_msg *)(NLMSG_DATA(messageBuffer));

    nlHdr->nlmsg_len = nlmsgSize;
    nlHdr->nlmsg_type = NLMSG_STOP_MESSAGE_TYPE;
    nlHdr->nlmsg_pid = KERNEL_PID + 1;

    cnMsg->id.idx = CN_IDX_PROC;    // idx and val are used for message routing
    cnMsg->id.val = CN_VAL_PROC;
    cnMsg->len = dataSize;
    memcpy(cnMsg->data, &op, dataSize);

    return sendNetlinkMessage(socket, messageBuffer, nlmsgSize, 0, CN_IDX_PROC);
}

bool ProcListener::receiveConnectorNetlinkMessage(int socket, void *buffer, size_t bufferSize)
{
    // the sender
    sockaddr_nl srcAddr;
    memset(&srcAddr, 0, sizeof(srcAddr));
    socklen_t addrLen = sizeof(srcAddr);

    // Receive the Netlink message into the buffer
    struct nlmsghdr* nl_hdr = (struct nlmsghdr*)buffer;

    ssize_t nBytes = recvfrom(socket, buffer, bufferSize, 0,
                             (sockaddr*)&srcAddr, &addrLen);
    if (nBytes <= 0) {
        errno_ = errno;
        ostringstream os;
        os << "PROCLISTENER receiveConnectorNetlinkMessage: nBytes is " << nBytes << ", errno is " << errno;
        cat_.debug(os.str());
        return false;
    }

    //cat_.debug("PROCLISTENER message received by thread from pid %u", (unsigned int)srcAddr.nl_pid);

    // While the nl_hdr points to a valid message, keep processing
    while (NLMSG_OK(nl_hdr, nBytes)) {

        // Handle NOOP and ERROR messages
        unsigned msg_type = nl_hdr->nlmsg_type;
        if (msg_type == NLMSG_NOOP) {
            continue;
        } else if (msg_type == NLMSG_ERROR || msg_type == NLMSG_OVERRUN) {
            errno = -EINVAL;
            return false;
        } else if (msg_type == NLMSG_STOP_MESSAGE_TYPE) {
            // this is our own message
            cat_.info("PROCLISTENER stop message received");
            stop_ = true;
            return true;
        }

        // Call our handler with the inner proc_event struct
        struct cn_msg* msg = (struct cn_msg*)(NLMSG_DATA(nl_hdr));
        forwardEvent(msg->data, nBytes);

        // Terminate if this was the last message
        if (msg_type == NLMSG_DONE) {
            break;
        }

        // Handle more messages if such exist
        nl_hdr = NLMSG_NEXT(nl_hdr, nBytes);
    }
    return true;
}

void ProcListener::forwardEvent(uint8_t *data, size_t len)
{
    using namespace rmcommon;

    struct proc_event *ev = reinterpret_cast<struct proc_event *>(data);

    switch (ev->what) {
    case PROC_EVENT_FORK:
        bus_.publish(new ForkEvent(data, len));
        break;
    case PROC_EVENT_EXEC:
        bus_.publish(new ExecEvent(data, len));
        break;
    case PROC_EVENT_EXIT:
        bus_.publish(new ExitEvent(data, len));
        break;
        /* Other event types: PROC_EVENT_NONE, PROC_EVENT_UID, PROC_EVENT_GID,
           PROC_EVENT_SID, PROC_EVENT_PTRACE, PROC_EVENT_COREDUMP */
    default:
        // ignore event
        break;
    }
}

void ProcListener::run()
{
    cat_.info("PROCLISTENER running");
    nl_socket_ = createNetlinkSocket();
    if (nl_socket_ == -1) {
        cat_.error("PROCLISTENER could not create Netlink socket %s", strerror(errno));
        return;
    }
    nl_pid_ = gettid();
    if (!bindNetlinkSocket(nl_socket_, nl_pid_)) {
        cat_.error("PROCLISTENER could not bind Netlink socket %s", strerror(errno));
        close(nl_socket_);
        nl_socket_ = -1;
        nl_pid_ = -1;
        return;
    }

    if (!sendConnectorNetlinkMessageToKernel(nl_socket_, LISTEN)) {
        cat_.error("PROCLISTENER could not register %s", strerror(errno));
        close(nl_socket_);
        nl_socket_ = -1;
        nl_pid_ = -1;
        return;
    }

    // main loop

    uint8_t buffer[1024];
    stop_ = false;
    while (!stop_) {
        if (!receiveConnectorNetlinkMessage(nl_socket_, buffer, sizeof(buffer))) {
            cat_.error("PROCLISTENER error in receiveMessage: exiting");
            break;
        }
    }

    // unregister

    sendConnectorNetlinkMessageToKernel(nl_socket_, IGNORE);

    cat_.info("PROCLISTENER exiting");
}

bool ProcListener::stop()
{
    // Sender

    cat_.debug("PROCLISTENER sending STOP message from thread %ld", (long)gettid());

    int sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
    if (sock == -1) {
        perror("PROCLISTENER socket");
        errno_ = errno;
        return false;
    }

#if 0
    // binding the socket is not needed

    struct sockaddr_nl srcAddr;
    memset(&srcAddr, 0, sizeof(srcAddr));
    srcAddr.nl_family = AF_NETLINK;
    srcAddr.nl_pid = KERNEL_PID;
    srcAddr.nl_groups = CN_IDX_PROC;
    int rc = bind(sock, (struct sockaddr *)&srcAddr, sizeof(srcAddr));
    if (rc) {
        perror("bind");
        errno_ = errno;
        close(sock);
        return false;
    }
#endif

    if (!sendConnectorNetlinkMessageToThread(sock, MessageData::STOP)) {
        cat_.error("PROCLISTENER could not send STOP message to thread");
        return false;
    } else {
        cat_.debug("PROCLISTENER STOP message successfully sent to thread");;
        return true;
    }
}

}   // namespace wm
