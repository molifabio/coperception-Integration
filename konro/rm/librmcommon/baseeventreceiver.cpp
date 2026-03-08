#include "baseeventreceiver.h"
#include "threadname.h"
#include <log4cpp/Category.hh>

using namespace std;

namespace rmcommon {

BaseEventReceiver::BaseEventReceiver(const char *threadName) :
    threadName_(threadName)
{
}

void BaseEventReceiver::addEvent(std::shared_ptr<const BaseEvent> event)
{
    queue_.push(event);
}

void BaseEventReceiver::run()
{
    setThreadName(threadName_.c_str());

    using Logger = log4cpp::Category;
    Logger::getRoot().info("%s starts", getThreadName().c_str());
    while (!stopped()) {
        shared_ptr<const rmcommon::BaseEvent> event;
        if (queue_.waitAndPop(event, WAIT_POP_TIMEOUT_MILLIS)) {
//            Logger::getRoot().info("BASEEEVENTRECEIVER received event %s",
//                                   event->getName().c_str());
            if (!processEvent(event))
                stop();
        } else {
            // Logger::getRoot().info("BaseEventReceiver: no event");
        }
    }
    Logger::getRoot().info("%s stops", getThreadName().c_str());
}


}   // namespace rmcommon
