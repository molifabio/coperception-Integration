#ifndef IBASEPOLICY_H
#define IBASEPOLICY_H

#include "../appmapping.h"
#include "platformdescription.h"
#include "monitorevent.h"
#include "feedbackevent.h"
#include <memory>

namespace rp {

/*!
 * \interface an interface for event-based scheduling.
 * Each event type in the system is associated to a handler function.
 */
class IBasePolicy {
protected:
    PlatformDescription platformDescription_;
public:
    explicit IBasePolicy() = default;
    virtual ~IBasePolicy() = default;

    virtual const char *name() = 0;

    /*!
     * Handles the addition of a new app to the system.
     */
    virtual void addApp(AppMappingPtr appMapping) = 0;

    /*!
     * Handles the removal of an app from the system.
     */
    virtual void removeApp(AppMappingPtr appMapping) = 0;

    /*!
     * Handles a timer event.
     */
    virtual void timer() = 0;

    /*!
     * Handles a platform monitor event.
     */
    virtual void monitor(std::shared_ptr<const rmcommon::MonitorEvent> event) = 0;

    /*!
     * Handles an application feedback event.
     *
     * If useful for the policy, the current feedback can be saved
     * in the AppMapping class.
     */
    virtual void feedback(AppMappingPtr appMapping, int feedback) = 0;
};

}   // namespace rp

#endif // IBASEPOLICY_H
