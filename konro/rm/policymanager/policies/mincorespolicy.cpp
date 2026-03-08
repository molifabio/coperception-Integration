#include "mincorespolicy.h"
#include <vector>
#include <algorithm>
#include <sstream>
#include <limits>
#include <cassert>
#include <log4cpp/Category.hh>

namespace rp {

MinCoresPolicy::MinCoresPolicy(const AppMappingSet &apps, PlatformDescription pd) :
    apps_(apps),
    platformDescription_(pd),
    hasLastPlatformLoad_(false),
    appsOnPu_(pd.getNumProcessingUnits(), 0)
{
}

/*! Counts the number of apps in the same cgroup of the specified one */
static int countAppsWithSameCgroup(const AppMappingSet &apps, AppMappingPtr appMapping) {
    int n = 0;
    for (const auto &am: apps) {
        if (am->getCgroupDir() == appMapping->getCgroupDir()) {
            ++n;
        }
    }
    return n;
}

/*!
 * Returns the PU that currently has lower usage.
 *
 * Tries to find a PU which has already some apps
 * handled by this policy on it.
 */
int MinCoresPolicy::getLowerUsagePU()
{
    if (hasLastPlatformLoad_) {
        // we'd better move this line and change the if condition
        const std::vector<int> &pus = lastPlatformLoad_.getPUs();
        if (pus.empty()) {
            return 0;
        }
        int minLoad = std::numeric_limits<int>::max();
        int minLoadIdx = -1;
        int minUsedLoad = std::numeric_limits<int>::max();
        int minUsedLoadIdx = -1;
        for (size_t i = 0; i < pus.size(); ++i) {
            if (pus[i] < minLoad) {
                minLoad = pus[i];
                minLoadIdx = (int)i;
            }
            if (appsOnPu_[i] > 0 && pus[i] < minUsedLoad) {
                minUsedLoad = pus[i];
                minUsedLoadIdx = (int)i;
            }
        }
        if (minUsedLoadIdx != -1) {
            return minUsedLoadIdx;
        } else if (minLoadIdx != -1) {
            return minLoadIdx;
        }
    } else {
        // we don't have a PlatformLoad: find the used PU with the least number of apps
        std::vector<int>::iterator minElem = std::min_element(appsOnPu_.begin(), appsOnPu_.end());
        if (minElem != appsOnPu_.end()) {
            // The ID of the PU is the index in the array
            return std::distance(appsOnPu_.begin(), minElem);
        }
    }
    return 0;
}

int MinCoresPolicy::getLowerUsagePU(const PUSet &puset)
{
    int res = -1;
    std::vector<short> vec = rmcommon::toVector(puset);
    const std::vector<int> &pus = lastPlatformLoad_.getPUs();
    if (hasLastPlatformLoad_ && !pus.empty()) {
        int minLoad = std::numeric_limits<int>::max();
        int minLoadIdx = -1;
        int minUsedLoad = std::numeric_limits<int>::max();
        int minUsedLoadIdx = -1;
        for (short i : vec) {
            assert(i >= 0 && i < pus.size());
            if (pus[i] < minLoad) {
                minLoad = pus[i];
                minLoadIdx = (int)i;
            }
            if (appsOnPu_[i] > 0 && pus[i] < minUsedLoad) {
                minUsedLoad = pus[i];
                minUsedLoadIdx = (int)i;
            }
        }
        if (minUsedLoadIdx != -1) {
            return minUsedLoadIdx;
        } else if (minLoadIdx != -1) {
            return minLoadIdx;
        }
    } else {
        // we don't have a PlatformLoad: find the used PU with the least number of apps
        int minAppsOnPu = std::numeric_limits<int>::max();
        int minAppsOnPuIdx = -1;
        for (short i : vec) {
            if (appsOnPu_[i] < minAppsOnPu) {
                minAppsOnPu = appsOnPu_[i];
                minAppsOnPuIdx = i;
            }
        }
        res = minAppsOnPuIdx;
        log4cpp::Category::getRoot().info("Intermediate result %d", res);
    }
    return res;
}

static void dumpSet(const char *name, const std::set<short> &s)
{
    std::ostringstream os;

    for (short n: s) {
        os << n << ' ';
    }
    log4cpp::Category::getRoot().info("%s %s", name, os.str().c_str());
}

static void dumpCpuSetVector(const char *name, const rmcommon::CpusetVector &vec)
{
    std::string s = rmcommon::toString(vec);
    log4cpp::Category::getRoot().info("%s %s", name, s.c_str());
}

/*! Picks which PU is the most appropriate to execute a new process.
    Initially, every process is always assigned to a single PU.
    The range of PUs available for a process can later be expanded. */
int MinCoresPolicy::pickInitialCpu()
{
    return getLowerUsagePU();
}

/*!
 * Returns a set with all the PUs not present in "vec"
 */
MinCoresPolicy::PUSet MinCoresPolicy::getAvailablePUs(const PUSet &usedPUs)
{
    PUSet allPUs = platformDescription_.getPUSet();
    PUSet res;
    std::set_difference(allPUs.begin(), allPUs.end(), usedPUs.begin(), usedPUs.end(), std::inserter(res, end(res)));

    dumpSet("allPUs : ", allPUs);
    dumpSet("usedPUs: ", usedPUs);
    dumpSet("res    : ", res);
    return res;
}

/*!
 * Returns a set containing all the PUs used by Konro
 */
MinCoresPolicy::PUSet MinCoresPolicy::getKonroUsedPUs() {
    PUSet res;
    for (int i = 0; i < appsOnPu_.size(); ++i) {
        if (appsOnPu_[i] > 0) {
            res.insert(i);
        }
    }
    return res;
}

/*!
 * Returns a set containing all the PUs used by Konro but
 * not present in "vec"
 */
MinCoresPolicy::PUSet MinCoresPolicy::getKonroAvailablePUs(const PUSet &usedPUs)
{
    PUSet konroUsedPUs = getKonroUsedPUs();
    PUSet res;
    std::set_difference(konroUsedPUs.begin(), konroUsedPUs.end(), usedPUs.begin(), usedPUs.end(), std::inserter(res, end(res)));

    dumpSet("konroUsedPUs : ", konroUsedPUs);
    dumpSet("usedPUs: ", usedPUs);
    dumpSet("res    : ", res);
    return res;
}

//short MinCoresPolicy::getNextPU(const rmcommon::CpusetVector &vec)
//{
//    PUSet availPUs = getAvailablePUs(rmcommon::toSet(vec));
//    if (availPUs.empty()) {
//        return -1;          // no free PUs
//    }
//    std::set<short> usedPUs = rmcommon::toSet(vec);
//    short bestPU = -1;
//    int bestPUdistance = numeric_limits<int>::max();
//    for (short usedPu: usedPUs) {
//        for (short availPu: availPUs) {
//            int distance = platformDescription_.getPUDistance(usedPu, availPu);
//            if (distance >= 0 && distance < bestPUdistance) {
//                bestPU = availPu;
//                bestPUdistance = distance;
//            }
//        }
//    }
//    return bestPU;
//}

/*!
 * \brief Gets the PUs with the shortest distance from the used ones
 * \param usedPUs the vector of currently used PUs
 * \param availPUs the vector of available PUs to choose from
 * \return the vector of PUs with the shortest distance
 */
MinCoresPolicy::PUSet MinCoresPolicy::getNearestPUs(PUSet usedPUs, PUSet availPUs) {
    PUSet res;
    int bestPUdistance = numeric_limits<int>::max();
    for (short usedPu: usedPUs) {
        for (short availPu: availPUs) {
            int distance = platformDescription_.getPUDistance(usedPu, availPu);
            if (distance >= 0) {
               if (distance < bestPUdistance) {
                   bestPUdistance = distance;
                   res.clear();
                   res.insert(availPu);
               } else if (distance == bestPUdistance) {
                   res.insert(availPu);
               }
            }
        }
    }
    return res;
}

/*!
 *
 * \param vec List of PUs used by an app
 */
short MinCoresPolicy::getNextPU(const rmcommon::CpusetVector &vec)
{
    short res = -1;
    // Set of PUs used by an application
    PUSet usedPUs = rmcommon::toSet(vec);
    // Set of all the PUs already used by Konro but not present in "vec"
    // i.e. not used by the application
    PUSet konroAvailablePUs = getKonroAvailablePUs(usedPUs);
    if (!konroAvailablePUs.empty()) {
        // Pick a PU already claimed by Konro
        PUSet pus = getNearestPUs(usedPUs, konroAvailablePUs);
        res = getLowerUsagePU(pus);
    } else {
        PUSet availPUs = getAvailablePUs(usedPUs);
        if (!availPUs.empty()) {
            // Pick a new PU to assign to Konro
            PUSet pus = getNearestPUs(usedPUs, availPUs);
            dumpSet("nearestPUs : ", pus);
            res = getLowerUsagePU(pus);
            log4cpp::Category::getRoot().info("Result is %d", res);
        }
    }
    return res;
}

void increaseCPUBandwidth(AppMappingPtr app, rmcommon::NumericValue curBand, int percentage) {
    app->setCpuMax(curBand*(1+percentage));
}

void decreaseCPUBandwidth(AppMappingPtr appMapping, int percentage) {
    rmcommon::NumericValue curBand = appMapping->getCpuMax();
    if (curBand.isMax())
        curBand = 100;
    int newBand = curBand - ((curBand * percentage) / 100);
    appMapping->setCpuMax(newBand);
    log4cpp::Category::getRoot().info("MINCORESPOLICY decrease CPU band to %d", newBand);
}

short MinCoresPolicy::pickWorstPU(const rmcommon::CpusetVector &vec)
{
    if (rmcommon::countPUs(vec) == 1) {
        return -1;          // can't reduce the number of PUs
    }
    // tranform to linear vector, i.e. "1-3,5" becomes "1,2,3,5"
    std::vector<short> vec1 = rmcommon::toVector(vec);
//    log4cpp::Category::getRoot().info("MINCORESPOLICY testing removal of PU. Number of PUs = %d", (int)vec1.size());
    int bestTotalDistance = std::numeric_limits<int>::max();
    int bestTotalDistanceIdx = -1;
    for (int removePUIdx = 0; removePUIdx < vec1.size(); ++removePUIdx) {
//        log4cpp::Category::getRoot().info("MINCORESPOLICY testing removal of PU %d", vec1[removePUIdx]);
        // calculate the sum of all distances between PUs
        // when removePUIdx is removed
        int totalDistance = 0;
        for (int j = 0; j < vec1.size(); ++j) {
            for (int k = j+1; k < vec1.size(); ++k) {
                if (j != removePUIdx && k != removePUIdx && j != k) {
                    int distance = platformDescription_.getPUDistance(vec1[j], vec1[k]);
                    //log4cpp::Category::getRoot().info("MINCORESPOLICY distance between PU %d and %d = %d",
                    //          vec1[j], vec1[k], distance);
                    totalDistance += distance;
                }
            }
        }
//        log4cpp::Category::getRoot().info("MINCORESPOLICY total distance without PU %d = %d",
//                                           vec1[removePUIdx], totalDistance);
        if (totalDistance < bestTotalDistance) {
            bestTotalDistance = totalDistance;
            bestTotalDistanceIdx = removePUIdx;
//            log4cpp::Category::getRoot().info("MINCORESPOLICY best total distance = %d without PU %d",
//                                               bestTotalDistance, vec1[removePUIdx]);
        }
    }
    return bestTotalDistanceIdx != -1 ? vec1[bestTotalDistanceIdx] : -1;
}

void MinCoresPolicy::addApp(AppMappingPtr appMapping)
{
    // If there are already other Apps in the same cgroup folder,
    // handle them as a group and do nothing here
    if (countAppsWithSameCgroup(apps_, appMapping) > 1) {
        log4cpp::Category::getRoot().debug("MINCORESPOLICY addApp to an already initialized cgroup");
        return;
    }

    pid_t pid = appMapping->getPid();
    try {
        short initialPU = pickInitialCpu();
        appMapping->setPuVector({{initialPU, initialPU}});
        ++appsOnPu_[initialPU];
        log4cpp::Category::getRoot().info("MINCORESPOLICY addApp PID %ld to PU %d", (long)pid, initialPU);
    } catch (exception &e) {
        // An exception can happen for a short lived process; the
        // process has died and the Workload Manager has already
        // removed the cgroup directory for the process, but the
        // Resource Policicy Manager has not yet received the
        // corresponding RemoveEvent from the WorkloadManager
        log4cpp::Category::getRoot().error("MINCORESPOLICY addApp PID %ld: EXCEPTION %s",
                                           (long)pid, e.what());
    }
}

void MinCoresPolicy::removeApp(AppMappingPtr appMapping)
{
    rmcommon::CpusetVector vec = appMapping->getPuVector();
    std::vector<short> vecPu = rmcommon::toVector(vec);
    for (short pu: vecPu) {
        --appsOnPu_[pu];
        appsOnPu_[pu] = max(appsOnPu_[pu], 0);
    }
}

void MinCoresPolicy::timer()
{
    // no action required
}

void MinCoresPolicy::monitor(std::shared_ptr<const rmcommon::MonitorEvent> event)
{
    lastPlatformLoad_ = event->getPlatformLoad();
    hasLastPlatformLoad_ = true;
}

void MinCoresPolicy::feedback(AppMappingPtr appMapping, int feedback)
{
    float lowerLimit = 100.0f * (1-slack_);
    float upperLimit = 100.0f * (1+slack_);
    int constant = 15;
    if (feedback < lowerLimit) {
        rmcommon::CpusetVector vec = appMapping->getPuVector();
        dumpCpuSetVector("usedPUs: ", vec);
        short newPU = getNextPU(vec);
        if (newPU != -1) {
            log4cpp::Category::getRoot().info("MINCORESPOLICY adding PU %d", newPU);
            rmcommon::addPU(vec, newPU);
            appMapping->setPuVector(vec);
            ++appsOnPu_[newPU];
            dumpCpuSetVector("newPUs: ", vec);
        } else {
            log4cpp::Category::getRoot().info("MINCORESPOLICY no new PU available for proc %d", (long)appMapping->getPid());
        }
    }
#if 0
            // TODO - remove - this is a test of PU removal
            if (rmcommon::countPUs(vec) == 3) {
                short pu = pickWorstPU(vec);
                log4cpp::Category::getRoot().info("MINCORESPOLICY removing PU %d", pu);
                rmcommon::removePU(vec, pu);
                appMapping->setPuVector(vec);
                dumpCpuSetVector("newPUs: ", vec);
                return;
            }
#endif
    else if (feedback > upperLimit) {
        decreaseCPUBandwidth(appMapping, constant);
    }
    appMapping->setLastFeedback(feedback);
}

}   // namespace rp
