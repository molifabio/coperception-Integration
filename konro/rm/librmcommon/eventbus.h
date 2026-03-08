#ifndef EVENTBUS_H
#define EVENTBUS_H

// This implementation was inspired by code from Niko Savas with the
// folllowing licence:
//
// MIT License
//
// Copyright (c) 2018 Niko Savas
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the folshalowing conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


// This is the interface for MemberFunctionHandler that each specialization will use

#include <vector>
#include <map>
#include <typeinfo>
#include <typeindex>
#include <type_traits>
#include <memory>
#include <iostream>
#include <mutex>

namespace rmcommon {

class IHandlerFunctionBase {
public:
    virtual ~IHandlerFunctionBase() = default;
    void exec(void *event) {
        call(event);
    }
private:
    // Implemented by MemberFunctionHandler
    virtual void call(void *event) = 0;
};

/*!
 * Stores a (pointer to) an object of type T and a pointer to a member function
 * of that object which receives a std::shared_ptr<EventType>
 */
template<class T, class EventType>
class MemberFunctionHandler : public IHandlerFunctionBase {
public:
    typedef void (T::*MemberFunction)(std::shared_ptr<const EventType>);

    MemberFunctionHandler(T *instance, MemberFunction memberFunction) :
        instance{ instance },
        memberFunction{ memberFunction }
    {}

    void call(void *p) override {
        // here we really know what p points to
        std::shared_ptr<const EventType> event = *reinterpret_cast<std::shared_ptr<const EventType> *>(p);
        (instance->*memberFunction)(event);
    }
private:
    // Pointer to class instance
    T *instance;

    // Pointer to member function
    MemberFunction memberFunction;
};


/*!
 * A very basic event bus implementation
 * \p
 * Usage example:
 * \code
 * class EventBase {};
 * class EventDerived1 : public EventBase {};
 * class EventDerived2 : public EventBase {};
 *
 * class MyEventDerived1Consumer {
 * public:
 *     // always receive a shared_ptr<>to the BaseEvent
 *     bool processEvent(std::shared_ptr<EventBase> event);
 * }
 *
 * EventBus bus;
 * MyEventDerived1Consumer consumer;
 *
 * bus.subscribe<MyEventDerived1Consumer, EventDerived1, EventBase>
 *                  (this, &MyEventDerived1Consumer::processEvent);
 *
 * // always publish a raw pointer. The event bus creates the shared_ptr<>
 * // which is passed to all observers
 * bus.publish(new EventDerived1());
 * \endcode
 */
class EventBus {
    using HandlerList = std::vector<IHandlerFunctionBase *>;
    std::map<std::type_index, HandlerList *> subscribers;
    std::mutex global_mutex;

public:
    explicit EventBus() {}
    ~EventBus() {
        {
            std::unique_lock<std::mutex> lck(global_mutex);
            for (auto &elem: subscribers) {
                //std::cout << "deleting " << elem.first.hash_code() << std::endl;
                if (elem.second != nullptr) {
                    for (auto &handler: *elem.second) {
                        //std::cout << "deleting handler\n";
                        delete handler;
                    }
                }
                delete elem.second;
            }
        }
    }

    // Disable copy and assignment
    EventBus(const EventBus &other) = delete;
    EventBus &operator=(const EventBus &other) = delete;
    EventBus(EventBus &&other) noexcept = delete;
    EventBus &operator=(EventBus &&other) noexcept = delete;

    /*!
     * Subscribes an observer of type T to receive events of type EventType
     *
     * \param instance the instance of the object of type T
     * \param memberFunction the function to call when an event of type
     *                       EventType is called
     */
    template<typename T, typename EventType, typename BaseEvent = EventType>
    void subscribe(T *instance, void (T::*memberFunction)(std::shared_ptr<const BaseEvent>)) {
        std::unique_lock<std::mutex> lck(global_mutex);
        HandlerList *handlers = subscribers[typeid(EventType)];

        if (handlers == nullptr) {
            // this is the first subscription for this event type
            handlers = new HandlerList();
            subscribers[typeid(EventType)] = handlers;
        }
        IHandlerFunctionBase *ptr = new MemberFunctionHandler<T, const BaseEvent>(instance, memberFunction);
        handlers->push_back(ptr);
    }

    /*!
     * Publishes an event of type EventType. All observers will receive the event.
     * Note that the observers are processed sequentially, i.e. observer2 will not
     * be called until observer1 has finished processing.
     *
     * \param event the event to publish. A shared_ptr of this event will be created
     *              from the event and sent to the receivers
     */
    template<typename EventType>
    void publish(EventType *event) {
        std::unique_lock<std::mutex> lck(global_mutex);
        HandlerList *handlers = subscribers[typeid(EventType)];
        if (handlers == nullptr) {
            return;         // no subscriber for this event type
        }
        // create the shared_ptr of the event that will be passed
        // to the receivers
        std::shared_ptr<const EventType> p{event}; // = std::shared_ptr<EventType>(event);
        for (auto handler : *handlers) {
            if (handler != nullptr) {
                handler->exec(&p);
            }
        }
    }
};

}   // namespace rmcommon
#endif // EVENTBUS_H
