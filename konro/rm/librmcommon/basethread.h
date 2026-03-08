#ifndef BASETHREAD_H
#define BASETHREAD_H

#include "threadname.h"
#include <string>
#include <thread>
#include <atomic>

namespace rmcommon {

class BaseThread {
    std::thread baseThread_;
    std::atomic_bool stop_;
public:
    BaseThread();
    virtual ~BaseThread();

    /*!
     * Sets the thread name. Must be called from the run function.
     * \param name
     */
    void setThreadName(const char *name) {
        rmcommon::setThreadName(name);
    }

    const std::string threadName() const {
        return rmcommon::getThreadName();
    }

    /*! Executes the run() function in a new thread */
    virtual void start();

    /*! Sets the stop_ flag to true */
    virtual void stop();

    virtual void run() = 0;

    void join();

    bool stopped() {
        return stop_;
    }
};

}   // namespace rmcommon

#endif // BASETHREAD_H
