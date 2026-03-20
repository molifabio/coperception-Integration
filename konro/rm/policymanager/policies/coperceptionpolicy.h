#ifndef COPERCEPTIONPOLICY_H
#define COPERCEPTIONPOLICY_H

#include "ibasepolicy.h"
#include <set>
#include <vector>

namespace rp {

/*!
 * \class CoperceptionPolicy
 * \brief Conceptual policy for cooperative perception workloads.
 *
 * Reacts to the proxy quality metric (recall × clamp(gts/dets))
 * sent as feedback from coperception via the Konro HTTP API.
 *
 * Behavior:
 *  - feedback < lowThreshold  → add cores progressively
 *  - feedback > highThreshold → reduce cores / bandwidth
 *  - feedback keeps declining despite max resources → release all
 *    extra resources (the problem is not compute-bound)
 */
class CoperceptionPolicy : public IBasePolicy {
    using PUSet = std::set<short>;

    const AppMappingSet &apps_;
    PlatformDescription platformDescription_;
    rmcommon::PlatformLoad lastPlatformLoad_;
    bool hasLastPlatformLoad_;

    /// Number of apps scheduled on each PU
    std::vector<int> appsOnPu_;

    /// Feedback thresholds (Konro scale 0-200, target = 100)
    static constexpr int kLowThreshold  = 90;   // below → need more resources
    static constexpr int kHighThreshold = 110;   // above → can release resources

    /// Consecutive low-feedback ticks before giving up (network-caused)
    static constexpr int kGiveUpTicks = 5;

    /// Per-app state to detect persistent degradation
    struct AppState {
        int consecutiveLow = 0;
        int prevFeedback   = -1;
        bool gaveUp        = false;
    };
    std::map<pid_t, AppState> appStates_;

    int getLowerUsagePU();
    PUSet getUsedPUs(const rmcommon::CpusetVector &vec);
    PUSet getAvailablePUs(const PUSet &usedPUs);
    short getNextPU(const rmcommon::CpusetVector &vec);
    short pickWorstPU(const rmcommon::CpusetVector &vec);

public:
    CoperceptionPolicy(const AppMappingSet &apps, PlatformDescription pd);

    // IBasePolicy interface
    const char *name() override { return "CoperceptionPolicy"; }
    void addApp(AppMappingPtr appMapping) override;
    void removeApp(AppMappingPtr appMapping) override;
    void timer() override;
    void monitor(std::shared_ptr<const rmcommon::MonitorEvent> event) override;
    void feedback(AppMappingPtr appMapping, int feedback) override;
};

}   // namespace rp

#endif // COPERCEPTIONPOLICY_H
