#include "workloadmanager.h"
#include "eventbus.h"
#include "timer.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <algorithm>
#include <linux/cn_proc.h>

using namespace std;

namespace wm {

/*!
 * Returns the name of a process, given the process PID
 *
 * The name is read from /proc/<pid>/cmdline.
 * If the name is not found, an empty string is returned,
 */
static string getProcessNameByPid(int pid)
{
    ostringstream os;
    os << "/proc/" << pid << "/cmdline";
    fstream fs(os.str());
    string cmdline;
    if (fs.is_open()) {
        if (getline(fs, cmdline)) {
            // get rid of embedded '\0'
            replace(cmdline.begin(), cmdline.end(), '\0', ' ');
        }
    }
    return cmdline;
}

/*!
 * Compares by PID two App ("less" function) handled
 * by shared pointers
 *
 * \param lhs the first app to compare
 * \param rhs the second app to compare
 * \return true if pid of lsh is < than pid of rhs
 */
static bool appComp(const shared_ptr<rmcommon::App> &lhs, const shared_ptr<rmcommon::App> &rhs)
{
    return lhs->getPid() < rhs->getPid();
}

WorkloadManager::WorkloadManager(rmcommon::EventBus &bus, pc::IPlatformControl &pc) :
    rmcommon::BaseEventReceiver("WORKLOADMANAGER"),
    bus_(bus),
    platformControl_(pc),
    cat_(log4cpp::Category::getRoot()),
    apps_(appComp)
{
    subscribeToEvents();
}

WorkloadManager::AppSet::iterator WorkloadManager::findAppByPid(pid_t pid)
{
    // create a temporary App as key for the search
    shared_ptr<rmcommon::App> key = rmcommon::App::makeApp(pid, rmcommon::App::AppType::UNKNOWN);
    return apps_.find(key);
}

WorkloadManager::AppSet::iterator WorkloadManager::findAppByNsPid(pid_t nspid, rmcommon::namespace_t ns)
{
    AppSet::iterator it;
    for (it = apps_.begin(); it != apps_.end(); ++it) {
        shared_ptr<rmcommon::App> app = *it;
        if (app->getNsPid() == nspid && app->getPidNamespace() == ns) {
            break;
        }
    }
    return it;
}

void WorkloadManager::subscribeToEvents()
{
    using namespace rmcommon;

    bus_.subscribe<WorkloadManager, ForkEvent, BaseEvent>(this, &WorkloadManager::addEvent);
    bus_.subscribe<WorkloadManager, ExecEvent, BaseEvent>(this, &WorkloadManager::addEvent);
    bus_.subscribe<WorkloadManager, ExitEvent, BaseEvent>(this, &WorkloadManager::addEvent);
    bus_.subscribe<WorkloadManager, AddRequestEvent, BaseEvent>(this, &WorkloadManager::addEvent);
    bus_.subscribe<WorkloadManager, FeedbackRequestEvent, BaseEvent>(this, &WorkloadManager::addEvent);
}

void WorkloadManager::add(shared_ptr<rmcommon::App> app)
{
#ifdef TIMING
    rmcommon::KonroTimer timer;
#endif

    if (!platformControl_.addApplication(app)) {
        cat_.error("WORKLOADMANAGER could not add application (pid=%ld)", (long)app->getPid());
        return;
    }

#ifdef TIMING
    rmcommon::KonroTimer::TimeUnit micros1 = timer.Elapsed();
    cat_.debug("WORKLOADMANAGER add timing: addApplication(pid=%ld) = %ld microseconds\n",
               (long)app->getPid(),
               (long)micros1.count());
#endif

    apps_.insert(app);

#ifdef TIMING
    timer.Restart();
#endif

    bus_.publish(new rmcommon::AddEvent(app));

#ifdef TIMING
    rmcommon::KonroTimer::TimeUnit micros2 = timer.Elapsed();
    cat_.debug("WORKLOADMANAGER add timing: publish(pid=%ld) = %ld microseconds\n",
               (long)app->getPid(), (long)micros2.count());
#endif
}

void WorkloadManager::remove(pid_t pid)
{
    AppSet::iterator it = findAppByPid(pid);
    if (it != end(apps_)) {
        bus_.publish(new rmcommon::RemoveEvent(*it));
        platformControl_.removeApplication(*it);
        apps_.erase(it);
    }
}

bool WorkloadManager::isInKonro(pid_t pid)
{
    return findAppByPid(pid) != end(apps_);
}

bool WorkloadManager::processEvent(std::shared_ptr<const rmcommon::BaseEvent> event)
{
    using namespace rmcommon;

    if (const ForkEvent *ev = dynamic_cast<const ForkEvent *>(event.get())) {
        processForkEvent(static_pointer_cast<const ForkEvent>(event));
    } else if (const ExecEvent *ev = dynamic_cast<const ExecEvent *>(event.get())) {
        processExecEvent(static_pointer_cast<const ExecEvent>(event));
    } else if (const ExitEvent *ev = dynamic_cast<const ExitEvent *>(event.get())) {
        processExitEvent(static_pointer_cast<const ExitEvent>(event));
    } else if (const AddRequestEvent *ev = dynamic_cast<const AddRequestEvent *>(event.get())) {
        processAddRequestEvent(static_pointer_cast<const AddRequestEvent>(event));
    } else if (const FeedbackRequestEvent *ev = dynamic_cast<const FeedbackRequestEvent *>(event.get())) {
        processFeedbackRequestEvent(static_pointer_cast<const FeedbackRequestEvent>(event));
    } else {
        cat_.error("WORKLOADMANAGER received wrong event: %s", event->getName().c_str());
    }
    return true;
}

void WorkloadManager::processForkEvent(std::shared_ptr<const rmcommon::ForkEvent> event)
{
    const struct proc_event *ev = reinterpret_cast<const struct proc_event *>(&event->data_[0]);

    // Note: parent_pid and parent_tgid refer to the new process’ current parent
    //       which isn’t necessarily the process which called fork and birthed a
    //       child
    // https://natanyellin.com/posts/understanding-netlink-process-connector-output/

    AppSet::iterator iter = findAppByPid(ev->event_data.fork.parent_pid);
    bool isParentInKonro = iter != apps_.end();
    if (isParentInKonro) {
        // Child app inherits type from parent
        rmcommon::App::AppType parentType = (*iter)->getAppType();
        shared_ptr<rmcommon::App> app = rmcommon::App::makeApp(ev->event_data.fork.child_pid, parentType);
        app->setName(getProcessNameByPid(app->getPid()));
        add(app);
        cat_.info(
            R"(WORKLOADMANAGER fork {"parent_pid":%ld,"parent_name":'%s',"child_pid":%ld,"child_tgid":%ld,"child_name":'%s'})",
                (long)ev->event_data.fork.parent_pid,
                getProcessNameByPid(ev->event_data.fork.parent_pid).c_str(),
                (long)ev->event_data.fork.child_pid,
                (long)ev->event_data.fork.child_tgid,
                getProcessNameByPid(ev->event_data.fork.child_pid).c_str()
            );
        dumpMonitoredApps();
    }
}

void WorkloadManager::processExecEvent(std::shared_ptr<const rmcommon::ExecEvent> event)
{
    const struct proc_event *ev = reinterpret_cast<const struct proc_event *>(&event->data_[0]);
    pid_t pid = ev->event_data.exec.process_pid;
    AppSet::iterator it = findAppByPid(pid);
    if (it != apps_.end()) {
        shared_ptr<rmcommon::App> app = *it;
        app->setName(getProcessNameByPid(pid));

        cat_.info(R"(WORKLOADMANAGER exec {"process_pid":%ld,"process_name":'%s',"process_tgid":%ld})",
                  (long)ev->event_data.exec.process_pid,
                  getProcessNameByPid(ev->event_data.exec.process_pid).c_str(),
                  (long)ev->event_data.exec.process_pid);
        dumpMonitoredApps();
    }
}

void WorkloadManager::processExitEvent(std::shared_ptr<const rmcommon::ExitEvent> event)
{
    const struct proc_event *ev = reinterpret_cast<const struct proc_event *>(&event->data_[0]);

    pid_t pid = ev->event_data.exit.process_pid;
    if (isInKonro(pid)) {
        remove(pid);

        cat_.info(R"(WORKLOADMANAGER exit {"process_pid":%ld,"process_name":%s,"process_tgid":%ld})",
                  (long)ev->event_data.exit.process_pid,
                  getProcessNameByPid(ev->event_data.exit.process_pid).c_str(),
                  (long)ev->event_data.exit.process_tgid);
        dumpMonitoredApps();
    }
}

void WorkloadManager::processAddRequestEvent(std::shared_ptr<const rmcommon::AddRequestEvent> event)
{
    if(!isInKonro(event->getApp()->getPid())) {
        add(event->getApp());

        cat_.info(R"(WORKLOADMANAGER AddRequest {"process_pid":%ld,"process_name":'%s'})",
                  (long)event->getApp()->getPid(),
                  event->getApp()->getName().c_str());
        dumpMonitoredApps();
    } else {
        cat_.error(R"(WORKLOADMANAGER AddRequest from process already in Konro {"process_pid":%ld,"process_name":'%s'})",
                  (long)event->getApp()->getPid(),
                  event->getApp()->getName().c_str());
    }
}

void WorkloadManager::processFeedbackRequestEvent(std::shared_ptr<const rmcommon::FeedbackRequestEvent> event)
{
    AppSet::iterator it;

    if (event->getPidNamespace() == 0) {
        // The process belongs to Konro's namespace
        it = findAppByPid(event->getPid());
    } else {
        it = findAppByNsPid(event->getPid(), event->getPidNamespace());
    }
    if (it != end(apps_)) {
        rmcommon::FeedbackEvent *feedbackEvent = new rmcommon::FeedbackEvent(*it, event->getFeedback());
        // propagate original event time point, in order to track how much time if took
        // to deliver the original message fron HTTP to PolicyManager
        feedbackEvent->setTimePoint(event->getTimePoint());
        bus_.publish(feedbackEvent);

        cat_.info(R"(WORKLOADMANAGER FeedbackRequest received {"name":"%s","process_pid":%ld,"namespace":%ld,"feedback_value":%d})",
                  (*it)->getName().c_str(),
                  (long)event->getPid(),
                  (long)event->getPidNamespace(),
                  event->getFeedback());
    } else {
        cat_.error(R"(WORKLOADMANAGER FeedbackRequest received from process not in Konro {"process_pid":%ld,"namespace":%ld,"feedback_value":%d})",
                  (long)event->getPid(),
                  (long)event->getPidNamespace(),
                  event->getFeedback());
    }
}

void WorkloadManager::dumpMonitoredApps()
{
    ostringstream os;
    os << "WORKLOADMANAGER monitoring PIDS [";
    bool first = true;
    for (auto &app: apps_) {
        if (first)
            first = false;
        else
            os << ",";
        os << app->getPid();
    }
    os << "]";
    cat_.info(os.str());
}

}   // namespace wm
