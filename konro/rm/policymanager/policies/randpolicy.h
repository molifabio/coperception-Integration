#ifndef RANDPOLICY_H
#define RANDPOLICY_H

#include "ibasepolicy.h"

namespace rp {

/*!
 * \class random resource management policy
 *
 * Assigns each new process to a random CPU core.
 */
class RandPolicy : public IBasePolicy {
    const AppMappingSet &apps_;
    PlatformDescription platformDescription_;
public:
    explicit RandPolicy(const AppMappingSet &apps, PlatformDescription pd);

    // IBasePolicy interface
    virtual const char *name() override {
        return "RandPolicy";
    }
    virtual void addApp(AppMappingPtr appMapping) override;
    virtual void removeApp(AppMappingPtr appMapping) override;
    virtual void timer() override;
    virtual void monitor(std::shared_ptr<const rmcommon::MonitorEvent> event) override;
    virtual void feedback(AppMappingPtr appMapping, int feedback) override;
};

}   // namespace rp

#endif // RANDPOLICY_H
