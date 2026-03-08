#include "puprogressivepolicy.h"
#include <vector>
#include <algorithm>

namespace rp {

PuProgressivePolicy::PuProgressivePolicy(const AppMappingSet &apps, PlatformDescription pd) :
    apps_(apps),
    platformDescription_(pd),
    appsOnPu_(pd.getNumProcessingUnits(), 0)
{
}

/*!
 * Counts the number of apps in the same cgroup of the specified one.
 * appMapping does not (yet) belong to apps.
 */
static int countAppsWithSameCgroup(const AppMappingSet &apps, AppMappingPtr appMapping)
{
    int n = std::count_if(apps.begin(), apps.end(),
                          [appMapping](AppMappingPtr am) {
                            return am->getCgroupDir() == appMapping->getCgroupDir();}
                          );
    return n;
}

/*!
 * Returns the PU that currently has lower usage.
 *
 * Tries to find a PU which has already some apps
 * handled by this policy on it, otherwise it returns
 * the PU with the lowest usage.
 */
int PuProgressivePolicy::getLowerUsagePU()
{
    const std::vector<int> &pus = lastPlatformLoad_.getPUs();
    if (!pus.empty()) {
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
        // we don't have PU load data: find the used PU with the least number of apps
        std::vector<int>::iterator minElem = std::min_element(appsOnPu_.begin(), appsOnPu_.end());
        if (minElem != appsOnPu_.end()) {
            // The ID of the PU is the index in the array
            return std::distance(appsOnPu_.begin(), minElem);
        }
    }
    return 0;
}

/*!
 * Finds the PU with the lowest load amongst the Pus
 * in "puset"
 *
 * \param puset the set of PUs to search
 * \return the PU with the lowest load in "puset"
 */
int PuProgressivePolicy::getLowerUsagePU(const PUSet &puset)
{
    int res = -1;
    std::vector<short> vec = rmcommon::toVector(puset);
    const std::vector<int> &pus = lastPlatformLoad_.getPUs();
    if (!pus.empty()) {
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
        // we don't have PU load data: find the used PU with the least number of apps
        int minAppsOnPu = std::numeric_limits<int>::max();
        int minAppsOnPuIdx = -1;
        for (short i : vec) {
            if (appsOnPu_[i] < minAppsOnPu) {
                minAppsOnPu = appsOnPu_[i];
                minAppsOnPuIdx = i;
            }
        }
        res = minAppsOnPuIdx;
        log4cpp::Category::getRoot().debug("PUPROGRESSIVEPOLICY Intermediate result %d", res);
    }
    return res;
}

/*!
 * Writes the set to the log file
*/
static void logSet(const char *name, const std::set<short> &s)
{
    std::ostringstream os;

    for (short n: s) {
        os << n << ' ';
    }
    log4cpp::Category::getRoot().debug("PUPROGRESSIVEPOLICY %s %s", name, os.str().c_str());
}

/*!
 * Writes the CpusetVector to the log file
 */
static void logCpuSetVector(const char *name, const rmcommon::CpusetVector &vec)
{
    std::string s = rmcommon::toString(vec);
    log4cpp::Category::getRoot().debug("PUPROGRESSIVEPOLICY %s %s", name, s.c_str());
}

/*!
 * Picks which PU is the most appropriate to execute a new process.
 * Initially, every process is always assigned to a single PU.
 * the range of PUs available for a process can later be expanded.
 */
int PuProgressivePolicy::pickInitialPU()
{
    return getLowerUsagePU();
}

/*!
 * Returns a set with all the PUs not present in "usedPUs"
 */
PuProgressivePolicy::PUSet PuProgressivePolicy::getAvailablePUs(const PUSet &usedPUs)
{
    PUSet allPUs = platformDescription_.getPUSet();
    PUSet res;
    std::set_difference(allPUs.begin(), allPUs.end(), usedPUs.begin(), usedPUs.end(), std::inserter(res, end(res)));

    logSet("PUPROGRESSIVEPOLICY allPUs : ", allPUs);
    logSet("PUPROGRESSIVEPOLICY usedPUs: ", usedPUs);
    logSet("PUPROGRESSIVEPOLICY res    : ", res);
    return res;
}

/*!
 * Returns a set containing all the PUs used by Konro
 */
PuProgressivePolicy::PUSet PuProgressivePolicy::getKonroUsedPUs()
{
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
PuProgressivePolicy::PUSet PuProgressivePolicy::getKonroAvailablePUs(const PUSet &usedPUs)
{
    PUSet konroUsedPUs = getKonroUsedPUs();
    PUSet res;
    std::set_difference(konroUsedPUs.begin(), konroUsedPUs.end(), usedPUs.begin(), usedPUs.end(), std::inserter(res, end(res)));

    logSet("konroUsedPUs : ", konroUsedPUs);
    logSet("usedPUs: ", usedPUs);
    logSet("res    : ", res);
    return res;
}

/*!
 * \brief Gets the PUs with the shortest distance from the used ones
 * \param usedPUs the vector of currently used PUs
 * \param availPUs the vector of available PUs to choose from
 * \return the vector of PUs with the shortest distance
 */
PuProgressivePolicy::PUSet PuProgressivePolicy::getNearestPUs(PUSet usedPUs, PUSet availPUs)
{
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
 * Gets the best new PU to assign to a process.
 * The best PU is the one with the shorter distance from the ones
 * in the vec parameter. If multiple PUs in the architecture have
 * the same shorter distance, we pick the one with less usage.
 * If no new PUs are available, the method returns -1.
 * \param vec the list of PUs currently used by an app
 * \return the best PU that can be granted to the app
 */
short PuProgressivePolicy::getNewPU(const rmcommon::CpusetVector &vec)
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
            logSet("PUPROGRESSIVEPOLICY nearestPUs : ", pus);
            res = getLowerUsagePU(pus);
            log4cpp::Category::getRoot().debug("PUPROGRESSIVEPOLICY result is %d", res);
        }
    }
    return res;
}

/*!
 * Tries to increase CPU bandwidth for the specified app without changing
 * the number of PUs it can use.
 * \param appMapping the app of interest
 * \param scalePercentage the percentage of quota increase (between 0.0 and 1.0)
 * \return true if bandwidth increase was successful, false otherwise
 */
static bool increaseCPUquota(AppMappingPtr appMapping, float scalePercentage)
{
    rmcommon::NumericValue curBand = appMapping->getCpuMax();
    if (curBand.isMax()) {
        log4cpp::Category::getRoot().debug("PUPROGRESSIVEPOLICY cpu already \"max\"");
        return false;
    } else {
        // max bandwidth achievable with the current number of assigned PUs
        int maxBand = appMapping->countPUs() * 100;
        if (curBand >= maxBand) {
            log4cpp::Category::getRoot().debug("PUPROGRESSIVEPOLICY cpu is max value (%d%%)", maxBand);
            return false;
        } else {
            int newBand = curBand * (1 + scalePercentage);
            newBand = std::min(newBand, maxBand);
            appMapping->setCpuMax(newBand);
            log4cpp::Category::getRoot().info("PUPROGRESSIVEPOLICY increase cpu.max from %d%% to %d%%",
                                              (int)curBand, newBand);
            return true;
        }
    }
}

/*!
 * Tries to decrease CPU bandwidth for the specified app without changing
 * the number of PUs it can use.
 * \param appMapping the app of interest
 * \param scalePercentage the percentage of quota decrease (between 0.0 and 1.0)
 * \return true if bandwidth decrease was successful, false otherwise
 */
static bool decreaseCPUquota(AppMappingPtr appMapping, float scalePercentage)
{
    rmcommon::NumericValue curBand = appMapping->getCpuMax();
    int numPUs = appMapping->countPUs();
    if (curBand.isMax())
        curBand = numPUs * 100;
    int minBand = (numPUs - 1) * 100;
    if (curBand <= minBand || curBand <= 0) {
        return false;
    } else {
        int newBand = curBand * (1 - scalePercentage);
        newBand = std::max(newBand, minBand);
        appMapping->setCpuMax(newBand);
        log4cpp::Category::getRoot().info("PUPROGRESSIVEPOLICY decrease CPU band from %d%% to %d%%",
                                          (int)curBand, newBand);
        return true;
    }
}

/*!
 * Gets the best PU to remove from a process.
 * The best PU is the one with the shorter distance from the ones
 * in the vec parameter. If multiple PUs in the architecture have
 * the same shorter distance, we pick the one with less usage.
 * If no new PUs are available, the method returns -1.
 * \param vec the list of PUs currently used by an app
 * \return the best PU that can be granted to the app
 */
short PuProgressivePolicy::pickWorstPU(const rmcommon::CpusetVector &vec)
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

void PuProgressivePolicy::addApp(AppMappingPtr appMapping)
{
    // If there are already other Apps in the same cgroup folder,
    // handle them as a group and do nothing here
    if (countAppsWithSameCgroup(apps_, appMapping) > 1) {
        log4cpp::Category::getRoot().debug("PUPROGRESSIVEPOLICY addApp to an already initialized cgroup");
        return;
    }

    pid_t pid = appMapping->getPid();
    try {
        int cpuMax = 100;
        short initialPU = pickInitialPU();
        appMapping->setPuVector({{initialPU, initialPU}});
        ++appsOnPu_[initialPU];
        appMapping->setCpuMax(cpuMax);
        log4cpp::Category::getRoot().info("PUPROGRESSIVEPOLICY addApp PID %ld to PU %d with CPU max=%d",
                                          (long)pid, initialPU, cpuMax);
    } catch (exception &e) {
        // An exception can happen for a short lived process; the
        // process has died and the Workload Manager has already
        // removed the cgroup directory for the process, but the
        // Resource Policicy Manager has not yet received the
        // corresponding RemoveEvent from the WorkloadManager
        log4cpp::Category::getRoot().error("PUPROGRESSIVEPOLICY addApp PID %ld: EXCEPTION %s",
                                           (long)pid, e.what());
    }
}

void PuProgressivePolicy::removeApp(AppMappingPtr appMapping)
{
    rmcommon::CpusetVector vec = appMapping->getPuVector();
    std::vector<short> vecPu = rmcommon::toVector(vec);
    for (short pu: vecPu) {
        --appsOnPu_[pu];
        appsOnPu_[pu] = max(appsOnPu_[pu], 0);
    }
}

void PuProgressivePolicy::timer()
{
    // no action required
}

void PuProgressivePolicy::monitor(std::shared_ptr<const rmcommon::MonitorEvent> event)
{
    lastPlatformLoad_ = event->getPlatformLoad();
}

void PuProgressivePolicy::feedback(AppMappingPtr appMapping, int feedback)
{
    // lower bound for application performance
    float lowerLimit = 100.0f * (1-slack_);
    // upper bound for application performance
    float upperLimit = 100.0f * (1+slack_);
    // multiplier for resource scaling
    float scalePercentage = 0.15;

    // in this case we try to improve app performance by assigning more resources
    if (feedback < lowerLimit) {
        // try to increase cpu bandwith without assigning more PUs
        if (increaseCPUquota(appMapping, scalePercentage))
            return;
        // try to increase assigned number of PUs otherwise
        rmcommon::CpusetVector vec = appMapping->getPuVector();
        logCpuSetVector("usedPUs: ", vec);
        short newPU = getNewPU(vec);
        if (newPU != -1) {
            log4cpp::Category::getRoot().info("PUPROGRESSIVEPOLICY adding PU %d", newPU);
            rmcommon::addPU(vec, newPU);
            appMapping->setPuVector(vec);
            ++appsOnPu_[newPU];
            logCpuSetVector("newPUs: ", vec);
            increaseCPUquota(appMapping, scalePercentage);
        } else {
            log4cpp::Category::getRoot().info("PUPROGRESSIVEPOLICY no new PU available for proc %d", (long)appMapping->getPid());
        }

      // in this case we try to reduce app performance to save resources
    } else if (feedback > upperLimit) {
        // try to decrease cpu bandwith without reducing number of PUs
        if (decreaseCPUquota(appMapping, scalePercentage))
            return;
        // try to reduce assigned number of PUs otherwise
        rmcommon::CpusetVector vec = appMapping->getPuVector();
        logCpuSetVector("usedPUs: ", vec);
        short remPU = pickWorstPU(vec);
        if (remPU != -1) {
            log4cpp::Category::getRoot().info("PUPROGRESSIVEPOLICY removing PU %d", remPU);
            rmcommon::removePU(vec, remPU);
            appMapping->setPuVector(vec);
            --appsOnPu_[remPU];
            logCpuSetVector("newPUs: ", vec);
            decreaseCPUquota(appMapping, scalePercentage);
        } else {
            log4cpp::Category::getRoot().info("PUPROGRESSIVEPOLICY no PU to remove for proc %d", (long)appMapping->getPid());
        }
    }
    appMapping->setLastFeedback(feedback);
}

}   // namespace rp
