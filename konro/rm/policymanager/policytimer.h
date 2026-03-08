#ifndef POLICYTIMER_H
#define POLICYTIMER_H

#include "eventbus.h"
#include "basethread.h"

namespace rp {

/*!
 * \brief a timer that periodically generates a TimerEvent to wake up the
 * resource policy at fixed temporal intervals.
 */
class PolicyTimer : public rmcommon::BaseThread {
    rmcommon::EventBus &bus_;
    int seconds_;

    virtual void run() override;
public:
    PolicyTimer(rmcommon::EventBus &eventBus, int seconds);

};

}   // namespace rp

#endif // POLICYTIMER_H
