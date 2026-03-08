#ifndef CPUTIMEDATA_H
#define CPUTIMEDATA_H

#include <iostream>
#include <string>
#include <cinttypes>

/*!
 * CPU times read from /proc/stat
 */
struct CPUTimeData {
    std::string name;
    // Time spent in user mode
    uint64_t usertime;
    // Time spent in user mode with low priority (nice)
    uint64_t nicetime;
    // Time spent in system mode
    uint64_t systemtime;
    // Time spent in the idle task
    uint64_t idletime;
    // Time waiting for I/O to complete
    uint64_t ioWait;
    // Time servicing interrupts
    uint64_t irq;
    // Time servicing softirqs
    uint64_t softIrq;
    // Stolen time (time spent in other OS when running in a virtualized environment)
    uint64_t steal;
    // Time spent running a virtual CPU for guest OS under the control of the kernel
    uint64_t guest;
    // Time spent running a niced guest
    uint64_t guestnice;
    // Sum of all the previous times
    uint64_t totaltime;

    CPUTimeData() {
        usertime = 0;
        nicetime = 0;
        systemtime = 0;
        idletime = 0;
        ioWait = 0;
        irq = 0;
        softIrq = 0;
        steal = 0;
        guest = 0;
        guestnice = 0;
        totaltime = 0;
    }

    CPUTimeData(
            std::string nameParam,
            uint64_t usertimeParam,
            uint64_t nicetimeParam,
            uint64_t systemtimeParam,
            uint64_t idletimeParam,
            uint64_t ioWaitParam,
            uint64_t irqParam,
            uint64_t softIrqParam,
            uint64_t stealParam,
            uint64_t guestParam,
            uint64_t guestniceParam) {

        name = nameParam;
        usertime = usertimeParam - guestParam;
        nicetime = nicetimeParam - guestniceParam;
        systemtime = systemtimeParam;
        idletime = idletimeParam;
        ioWait = ioWaitParam;
        irq = irqParam;
        softIrq = softIrqParam;
        steal = stealParam;
        guest = guestParam;
        guestnice = guestniceParam;
        totaltime = usertime + nicetime + systemtime + irq + softIrq + idletime + ioWait + steal + guest + guestnice;
    }

    friend std::ostream &operator <<(std::ostream &os, const CPUTimeData &ctd) {
        os << '{'
           << "\"name\":" << '"' << ctd.name << '"'
           << ",\"usertime\":" << ctd.usertime
           << ",\"nicetime\":" << ctd.nicetime
           << ",\"systemtime\":" << ctd.systemtime
           << ",\"idletime\":" << ctd.idletime
           << ",\"ioWait\":" << ctd.ioWait
           << ",\"irq\":" << ctd.irq
           << ",\"steal\":" << ctd.steal
           << ",\"guest\":" << ctd.guest
           << ",\"guestnice\":" << ctd.guestnice
           << ",\"totaltime\":" << ctd.totaltime
           << '}';
        return os;
    }
};

#endif // CPUTIMEDATA_H
