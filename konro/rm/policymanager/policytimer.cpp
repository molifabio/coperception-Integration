#include "policytimer.h"
#include "timerevent.h"
#include "log4cpp/Category.hh"
#include <thread>

namespace rp {

using namespace std;

PolicyTimer::PolicyTimer(rmcommon::EventBus &eventBus, int seconds) :
    rmcommon::BaseThread(),
    bus_(eventBus),
    seconds_(seconds)
{

}

void PolicyTimer::run()
{
    setThreadName("POLICYTIMER");
    log4cpp::Category::getRoot().info("POLICYTIMER starting");
    while (!stopped()) {
        for (int i = 0; i < seconds_ && !stopped() ; ++i) {
            this_thread::sleep_for(chrono::seconds(1));
        }
        if (!stopped()) {
            bus_.publish(new rmcommon::TimerEvent());
        }
    }
    log4cpp::Category::getRoot().info("POLICYTIMER exiting");
}

}   // namespace rp
