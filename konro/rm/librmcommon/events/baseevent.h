#ifndef BASEEVENT_H
#define BASEEVENT_H

#include "../timer.h"
#include <iostream>
#include <string>

namespace rmcommon {

/*!
 * \class a generic event related to a process managed by Konro
 */
class BaseEvent {
    std::string name_;
    KonroTimer::TimePoint t_;           // event creation
public:
    BaseEvent(const char *name) :
        name_(name),
        t_(KonroTimer::now()) {
    }
    virtual ~BaseEvent() {}

    BaseEvent(const BaseEvent &other) = delete;
    BaseEvent &operator=(const BaseEvent &other) = delete;
    BaseEvent(BaseEvent &&other) = delete;
    BaseEvent &operator=(BaseEvent &&other) = delete;

    std::string getName() const {
        return name_;
    }

    void setTimePoint(const KonroTimer::TimePoint &t) {
        t_ = t;
    }

    KonroTimer::TimePoint getTimePoint() const {
        return t_;
    }

    virtual void printOnOstream(std::ostream &os) const {
        os << "{}";
    }

    friend std::ostream &operator <<(std::ostream &os, const BaseEvent &event) {
        event.printOnOstream(os);
        return os;
    }
};

}   // namespace rmcommon


#endif // BASEEVENT_H
