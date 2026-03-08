#include "coperceptionpolicy.h"
#include <algorithm>
#include <limits>
#include <sstream>
#include <log4cpp/Category.hh>

namespace rp {

CoperceptionPolicy::CoperceptionPolicy(const AppMappingSet &apps, PlatformDescription pd) :
    apps_(apps),
    platformDescription_(pd),
    hasLastPlatformLoad_(false),
    appsOnPu_(pd.getNumProcessingUnits(), 0)
{
}

// ── helpers ──────────────────────────────────────────────────────────

int CoperceptionPolicy::getLowerUsagePU()
{
    if (hasLastPlatformLoad_) {
        const std::vector<int> &pus = lastPlatformLoad_.getPUs();
        if (pus.empty())
            return 0;
        int minLoad = std::numeric_limits<int>::max();
        int minIdx  = 0;
        for (size_t i = 0; i < pus.size(); ++i) {
            if (pus[i] < minLoad) {
                minLoad = pus[i];
                minIdx  = static_cast<int>(i);
            }
        }
        return minIdx;
    }
    // Fallback: pick PU with fewest managed apps
    auto it = std::min_element(appsOnPu_.begin(), appsOnPu_.end());
    return static_cast<int>(std::distance(appsOnPu_.begin(), it));
}

CoperceptionPolicy::PUSet
CoperceptionPolicy::getUsedPUs(const rmcommon::CpusetVector &vec)
{
    return rmcommon::toSet(vec);
}

CoperceptionPolicy::PUSet
CoperceptionPolicy::getAvailablePUs(const PUSet &usedPUs)
{
    PUSet allPUs = platformDescription_.getPUSet();
    PUSet res;
    std::set_difference(allPUs.begin(), allPUs.end(),
                        usedPUs.begin(), usedPUs.end(),
                        std::inserter(res, res.end()));
    return res;
}

short CoperceptionPolicy::getNextPU(const rmcommon::CpusetVector &vec)
{
    PUSet usedPUs  = getUsedPUs(vec);
    PUSet availPUs = getAvailablePUs(usedPUs);
    if (availPUs.empty())
        return -1;

    // Pick the available PU closest to the ones already in use
    short bestPU   = -1;
    int   bestDist = std::numeric_limits<int>::max();
    for (short used : usedPUs) {
        for (short avail : availPUs) {
            int d = platformDescription_.getPUDistance(used, avail);
            if (d >= 0 && d < bestDist) {
                bestDist = d;
                bestPU   = avail;
            }
        }
    }
    return bestPU;
}

short CoperceptionPolicy::pickWorstPU(const rmcommon::CpusetVector &vec)
{
    if (rmcommon::countPUs(vec) <= 1)
        return -1;

    std::vector<short> pus = rmcommon::toVector(vec);
    // Remove the PU whose removal minimises total pairwise distance
    int bestTotal = std::numeric_limits<int>::max();
    int bestIdx   = -1;
    for (size_t r = 0; r < pus.size(); ++r) {
        int total = 0;
        for (size_t j = 0; j < pus.size(); ++j) {
            for (size_t k = j + 1; k < pus.size(); ++k) {
                if (j != r && k != r) {
                    int d = platformDescription_.getPUDistance(pus[j], pus[k]);
                    if (d >= 0) total += d;
                }
            }
        }
        if (total < bestTotal) {
            bestTotal = total;
            bestIdx   = static_cast<int>(r);
        }
    }
    return bestIdx >= 0 ? pus[bestIdx] : -1;
}

// ── IBasePolicy implementation ──────────────────────────────────────

void CoperceptionPolicy::addApp(AppMappingPtr appMapping)
{
    pid_t pid = appMapping->getPid();
    try {
        short pu = static_cast<short>(getLowerUsagePU());
        appMapping->setPuVector({{pu, pu}});
        ++appsOnPu_[pu];
        appStates_[pid] = {};
        log4cpp::Category::getRoot().info(
            "COPERCEPTIONPOLICY addApp PID %ld to PU %d", (long)pid, pu);
    } catch (std::exception &e) {
        log4cpp::Category::getRoot().error(
            "COPERCEPTIONPOLICY addApp PID %ld: EXCEPTION %s", (long)pid, e.what());
    }
}

void CoperceptionPolicy::removeApp(AppMappingPtr appMapping)
{
    std::vector<short> pus = rmcommon::toVector(appMapping->getPuVector());
    for (short pu : pus) {
        --appsOnPu_[pu];
        appsOnPu_[pu] = std::max(appsOnPu_[pu], 0);
    }
    appStates_.erase(appMapping->getPid());
}

void CoperceptionPolicy::timer()
{
    // No periodic action needed — decisions are feedback-driven.
}

void CoperceptionPolicy::monitor(std::shared_ptr<const rmcommon::MonitorEvent> event)
{
    lastPlatformLoad_ = event->getPlatformLoad();
    hasLastPlatformLoad_ = true;
}

/*!
 * Core decision logic.
 *
 * Three cases:
 *  1. feedback < kLowThreshold  → performance is degrading
 *     a. If not yet gave up → add one more PU
 *     b. If consecutive low ticks >= kGiveUpTicks while already at
 *        max PUs or after expansion didn't help → give up and
 *        release extra PUs (the bottleneck is the network, not CPU)
 *  2. feedback in [kLowThreshold, kHighThreshold] → steady, do nothing
 *  3. feedback > kHighThreshold → overprovisioned, remove one PU
 */
void CoperceptionPolicy::feedback(AppMappingPtr appMapping, int feedbackVal)
{
    pid_t pid = appMapping->getPid();
    AppState &st = appStates_[pid];

    log4cpp::Category::getRoot().info(
        "COPERCEPTIONPOLICY feedback PID %ld val=%d (prev=%d, consLow=%d, gaveUp=%d)",
        (long)pid, feedbackVal, st.prevFeedback, st.consecutiveLow, st.gaveUp);

    if (feedbackVal < kLowThreshold) {
        // ── Underperforming ──
        ++st.consecutiveLow;

        if (st.gaveUp) {
            // Already decided the problem is not CPU → do nothing
            log4cpp::Category::getRoot().info(
                "COPERCEPTIONPOLICY PID %ld: gave up, skipping (network-bound)", (long)pid);
        } else if (st.consecutiveLow >= kGiveUpTicks) {
            // Persistent degradation despite resource expansion → give up
            // Release all extra PUs back to a single core
            short initialPU = static_cast<short>(getLowerUsagePU());
            rmcommon::CpusetVector oldVec = appMapping->getPuVector();
            std::vector<short> oldPUs = rmcommon::toVector(oldVec);
            for (short pu : oldPUs) {
                --appsOnPu_[pu];
                appsOnPu_[pu] = std::max(appsOnPu_[pu], 0);
            }
            appMapping->setPuVector({{initialPU, initialPU}});
            ++appsOnPu_[initialPU];
            st.gaveUp = true;
            log4cpp::Category::getRoot().info(
                "COPERCEPTIONPOLICY PID %ld: giving up, reset to PU %d (network-bound)",
                (long)pid, initialPU);
        } else {
            // Try adding one more PU
            rmcommon::CpusetVector vec = appMapping->getPuVector();
            short newPU = getNextPU(vec);
            if (newPU != -1) {
                rmcommon::addPU(vec, newPU);
                appMapping->setPuVector(vec);
                ++appsOnPu_[newPU];
                log4cpp::Category::getRoot().info(
                    "COPERCEPTIONPOLICY PID %ld: adding PU %d (now %d PUs)",
                    (long)pid, newPU, rmcommon::countPUs(vec));
            } else {
                log4cpp::Category::getRoot().info(
                    "COPERCEPTIONPOLICY PID %ld: no free PU available", (long)pid);
            }
        }
    } else if (feedbackVal > kHighThreshold) {
        // ── Overperforming → release one PU ──
        st.consecutiveLow = 0;
        st.gaveUp = false;

        rmcommon::CpusetVector vec = appMapping->getPuVector();
        if (rmcommon::countPUs(vec) > 1) {
            short worst = pickWorstPU(vec);
            if (worst != -1) {
                rmcommon::removePU(vec, worst);
                appMapping->setPuVector(vec);
                --appsOnPu_[worst];
                appsOnPu_[worst] = std::max(appsOnPu_[worst], 0);
                log4cpp::Category::getRoot().info(
                    "COPERCEPTIONPOLICY PID %ld: removed PU %d (now %d PUs)",
                    (long)pid, worst, rmcommon::countPUs(vec));
            }
        }
    } else {
        // ── In acceptable range → reset counters ──
        st.consecutiveLow = 0;
        if (st.gaveUp) {
            // If performance recovered, allow future expansion again
            st.gaveUp = false;
        }
    }

    st.prevFeedback = feedbackVal;
    appMapping->setLastFeedback(feedbackVal);
}

}   // namespace rp
