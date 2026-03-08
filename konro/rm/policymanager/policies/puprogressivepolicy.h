#ifndef PUPROGRESSIVEPOLICY_H
#define PUPROGRESSIVEPOLICY_H

#include "ibasepolicy.h"

namespace rp {

class PuProgressivePolicy : public IBasePolicy {
    using PUSet = std::set<short>;

    const AppMappingSet &apps_;
    PlatformDescription platformDescription_;
    rmcommon::PlatformLoad lastPlatformLoad_;
    // Acceptable performace slack for applications (in percentage)
    float slack_ = 0.2f;
    // Number of apps scheduled on each PU
    std::vector<int> appsOnPu_;

    int getLowerUsagePU();
    int pickInitialPU();
    PUSet getAvailablePUs(const PUSet &usedPUs);
    //short getNextPU(const rmcommon::CpusetVector &vec);
    short pickWorstPU(const rmcommon::CpusetVector &vec);

    PUSet getKonroUsedPUs();
    PUSet getKonroAvailablePUs(const PUSet &vec);
    PUSet getNearestPUs(PUSet usedPUs, PUSet availPUs);
    short getNewPU(const rmcommon::CpusetVector &vec);
    int getLowerUsagePU(const PUSet &puset);
public:
    PuProgressivePolicy(const AppMappingSet &apps, PlatformDescription pd);

    // IBasePolicy interface
    virtual const char *name() override {
        return "PuProgressivePolicy";
    }
    virtual void addApp(AppMappingPtr appMapping) override;
    virtual void removeApp(AppMappingPtr appMapping) override;
    virtual void timer() override;
    virtual void monitor(std::shared_ptr<const rmcommon::MonitorEvent> event) override;
    virtual void feedback(AppMappingPtr appMapping, int feedback) override;
};

}   // namespace rp

#endif // PUPROGRESSIVEPOLICY_H
