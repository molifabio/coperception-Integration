#ifndef WORKLOADMANAGER_H
#define WORKLOADMANAGER_H

#include "app.h"
#include "iplatformcontrol.h"
#include "baseeventreceiver.h"
#include "baseevent.h"
#include "forkevent.h"
#include "execevent.h"
#include "exitevent.h"
#include "addevent.h"
#include "removeevent.h"
#include "feedbackevent.h"
#include "addrequestevent.h"
#include "feedbackrequestevent.h"
#include "namespaces.h"
#include <log4cpp/Category.hh>
#include <set>
#include <memory>
#include <sys/types.h>

namespace rmcommon {
    class EventBus;
}

namespace wm {
/*!
 * \brief a class for handling and manipulating a set of applications
 */
class WorkloadManager : public rmcommon::BaseEventReceiver {
    rmcommon::EventBus &bus_;
    pc::IPlatformControl &platformControl_;
    log4cpp::Category &cat_;

    /*! Comparison function for the set */
    using AppComparator = bool (*)(const std::shared_ptr<rmcommon::App> &lhs,
                                   const std::shared_ptr<rmcommon::App> &rhs);

    using AppSet = std::set<std::shared_ptr<rmcommon::App>, AppComparator>;
    AppSet apps_;

    /*!
     * Returns an iterator to the app with the specified pid from the set
     * of Konro's applications. If iter == apps_.end(), the app with the
     * desired PID is not present
     *
     * \param pid the pid of the application of interest
     * \return AppSet::iterator
     */
    AppSet::iterator findAppByPid(pid_t pid);

    AppSet::iterator findAppByNsPid(pid_t nspid, rmcommon::namespace_t ns);

    /*!
     * Processes a fork event.
     *
     * If a new process was forked by another process already handled by Konro,
     * the forked process is added to Konro.
     */
    void processForkEvent(std::shared_ptr<const rmcommon::ForkEvent> event);

    /*!
     * Processes an exec event.
     *
     * When a process handled by Konro performs an exec (often following a fork),
     * the application name is substituted with the name of the new program.
     */
    void processExecEvent(std::shared_ptr<const rmcommon::ExecEvent> event);

    /*!
     * Processes an exit event.
     *
     * If the exiting process was handled by Konro, the process is removed
     * from Konro's management.
     */
    void processExitEvent(std::shared_ptr<const rmcommon::ExitEvent> event);

    /*!
     * Processes a request to add a new event to Konro.
     *
     * When a new add reqeust is received, the necessary actions are
     * performed in the Platfrom Control layer and a new event is published
     * to the Event Bus to notify the Polcy Manager.
     */
    void processAddRequestEvent(std::shared_ptr<const rmcommon::AddRequestEvent> event);

    /*!
     * Processes a feedback message received from an integrated application.
     *
     * When a new feedback message is received by an integrated application,
     * the WorkloadManger checks if the pid in the message belongs to an app
     * handled by Konro. If so, a new event is published to the Event Bus
     * to notify the Polcy Manager.
     */
    void processFeedbackRequestEvent(std::shared_ptr<const rmcommon::FeedbackRequestEvent> event);

    void dumpMonitoredApps();

    /*!
     * Adds the specified application under the management of Konro.
     * \param app the application to add
     */
    void add(std::shared_ptr<rmcommon::App> app);

    /*!
     * Removes the specified application from the management of Konro.
     * \param app the application to remove
     */
    void remove(std::shared_ptr<rmcommon::App> app);

    /*!
     * Removes the application with the specified pid from the management of Konro.
     * \param pid the pid of the application to remove
     */
    void remove(pid_t pid);

    /*!
     * Checks if the application with the specified pid is under Konro's management.
     * \param pid the pid of the application of interest
     * \return true if the app is managed by Konro
     */
    bool isInKonro(pid_t pid);

    /*!
     * Subscribes to the relevant events from the EventBus.
     */
    void subscribeToEvents();

public:
    WorkloadManager(rmcommon::EventBus &bus, pc::IPlatformControl &pc);

    virtual bool processEvent(std::shared_ptr<const rmcommon::BaseEvent> event) override;
};

}   // namespace wm

#endif // WORKLOADMANAGER_H
