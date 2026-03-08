#ifndef MINCORESPOLICY_H
#define MINCORESPOLICY_H

#include "ibasepolicy.h"
#include <set>
#include <vector>

namespace rp {

class MinCoresPolicy : public IBasePolicy {
    using PUSet = std::set<short>;

    const AppMappingSet &apps_;
    PlatformDescription platformDescription_;
    rmcommon::PlatformLoad lastPlatformLoad_;
    bool hasLastPlatformLoad_;
    // Acceptable performace slack for applications (in percentage)
    float slack_ = 0.1;
    // Number of apps scheduled on each PU
    std::vector<int> appsOnPu_;

    int getLowerUsagePU();
    int pickInitialCpu();
    PUSet getAvailablePUs(const PUSet &usedPUs);
    //short getNextPU(const rmcommon::CpusetVector &vec);
    short pickWorstPU(const rmcommon::CpusetVector &vec);

    PUSet getKonroUsedPUs();
    PUSet getKonroAvailablePUs(const PUSet &vec);
    PUSet getNearestPUs(PUSet usedPUs, PUSet availPUs);
    short getNextPU(const rmcommon::CpusetVector &vec);
    int getLowerUsagePU(const PUSet &puset);

public:
    MinCoresPolicy(const AppMappingSet &apps, PlatformDescription pd);

    // IBasePolicy interface
    virtual const char *name() override {
        return "MinCoresPolicy";
    }
    virtual void addApp(AppMappingPtr appMapping) override;
    virtual void removeApp(AppMappingPtr appMapping) override;
    virtual void timer() override;
    virtual void monitor(std::shared_ptr<const rmcommon::MonitorEvent> event) override;
    virtual void feedback(AppMappingPtr appMapping, int feedback) override;
};

}   // namespace rp

#endif // MINCORESPOLICY_H

