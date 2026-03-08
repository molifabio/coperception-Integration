#include "randpolicy.h"
#include "cpusetcontrol.h"
#include "cpusetvector.h"
#include <iostream>
#include <sstream>
#include <random>
#include <utility>
#include <log4cpp/Category.hh>

using namespace std;

namespace rp {

namespace rputil {

    int countAppsWithSameCgroup(const AppMappingSet &apps, AppMappingPtr appMapping) {
        int n = 0;
        for (const auto &am: apps) {
            if (am->getCgroupDir() == appMapping->getCgroupDir()) {
                ++n;
            }
        }
        return n;
    }

}   // namespace rputil

/*!
 * Extracts a random CPU number
 *
 * \param cpusNum the number of CPUs avaiable on the machine
 * \return a random CPU number
 */
int getRandNumber(int cpusNum)
{
    std::random_device rd;
    std::mt19937 rng(rd()); // Mersenne-Twister generator
    std::uniform_int_distribution<int> uni(0, cpusNum-1);
    return uni(rng);
}

RandPolicy::RandPolicy(const AppMappingSet &apps, PlatformDescription pd) :
    apps_(apps),
    platformDescription_(pd)
{
}

void RandPolicy::addApp(AppMappingPtr appMapping)
{
    // If there are already other Apps in the same cgroup folder,
    // handle them as a group and do nothing here
    if (rputil::countAppsWithSameCgroup(apps_, appMapping) > 1) {
        log4cpp::Category::getRoot().debug("RANDPOLICY addApp to an already initialized cgroup");
        return;
    }

    pid_t pid = appMapping->getPid();
    try {
        short puNum = getRandNumber(platformDescription_.getNumProcessingUnits());
        log4cpp::Category::getRoot().debug("RANDPOLICY addApp PID %ld to PU %d", (long)pid, puNum);
        appMapping->setPuVector({{puNum, puNum}});
    } catch (exception &e) {
        // An exception can happen for a short lived process; the
        // process has died and the Workload Manager has already
        // removed the cgroup directory for the process, but the
        // Resource Policicy Manager has not yet received the
        // corresponding RemoveEvent from the WorkloadManager
        log4cpp::Category::getRoot().error("RANDPOLICY addApp PID %ld: EXCEPTION %s",
                                           (long)pid, e.what());
    }
}

void RandPolicy::removeApp(AppMappingPtr appMapping)
{
    // no action required
}

void RandPolicy::timer()
{
    // no action required
}

void RandPolicy::monitor(std::shared_ptr<const rmcommon::MonitorEvent> event)
{
    // no action required
}

void RandPolicy::feedback(AppMappingPtr appMapping, int feedback)
{
    appMapping->setLastFeedback(feedback);

    // no action required
}

}   // namespace rp
