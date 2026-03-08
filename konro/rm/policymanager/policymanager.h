#ifndef POLICYMANAGER_H
#define POLICYMANAGER_H

#include "baseeventreceiver.h"
#include "threadsafequeue.h"
#include "baseevent.h"
#include "addevent.h"
#include "removeevent.h"
#include "timerevent.h"
#include "monitorevent.h"
#include "feedbackevent.h"
#include "appmapping.h"
#include "policies/ibasepolicy.h"
#include "platformdescription.h"
#include <log4cpp/Category.hh>
#include <set>
#include <memory>
#include <thread>
#include <atomic>

namespace rmcommon {
class EventBus;
}

namespace rp {

/*!
 * Using the EventBus, PolicyManager receives events from multiple
 * threads, adds/removes managed applications to
 * a set and forwards the events to the chosen resource
 * policy.
 */
class PolicyManager : public rmcommon::BaseEventReceiver {
public:
    enum class Policy {
        NoPolicy,
        RandPolicy,
        PuProgressivePolicy,
        MinCoresPolicy,
        CoperceptionPolicy
    };

private:
    log4cpp::Category &cat_;
    rmcommon::EventBus &bus_;
    std::unique_ptr<IBasePolicy> policy_;
    PlatformDescription platformDescription_;
    AppMappingSet apps_;

    void subscribeToEvents();

    /*!
     * Processes a generic event by calling the appropriate handler function.
     * \param event the event to process
     */
    bool processEvent(std::shared_ptr<const rmcommon::BaseEvent> event) override;

    /*!
     * Processes an AddEvent.
     * \param event the event to process
     */
    void processAddEvent(std::shared_ptr<const rmcommon::AddEvent> event);

    /*!
     * Processes a RemoveEvent.
     * \param event the event to process
     */
    void processRemoveEvent(std::shared_ptr<const rmcommon::RemoveEvent> event);

    /*!
     * Processes a TimerEvent
     * \param ev the event to process
     */
    void processTimerEvent(std::shared_ptr<const rmcommon::TimerEvent> event);

    /*!
     * Processes a MonitorEvent
     * \param ev the event to process
     */
    void processMonitorEvent(std::shared_ptr<const rmcommon::MonitorEvent> event);

    /*!
     * Processes a FeedbackEvent
     * \param ev the event to process
     */
    void processFeedbackEvent(std::shared_ptr<const rmcommon::FeedbackEvent> event);

    /* for debugging */
    void dumpApps() const;

    /*! Factory method */
    std::unique_ptr<IBasePolicy> makePolicy(Policy policy);
public:

    PolicyManager(rmcommon::EventBus &bus, PlatformDescription pd, Policy policy = Policy::NoPolicy);
    virtual ~PolicyManager() = default;

    /*!
     * Return the policy with the specified name.
     * If no policy exists with that name, NoPolicy is returned.
     */
    static Policy getPolicyByName(const std::string &policyName);
};

}   // namespace rp

#endif // POLICYMANAGER_H
