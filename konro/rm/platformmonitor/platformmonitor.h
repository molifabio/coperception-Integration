#ifndef PLATFORMMONITOR_H
#define PLATFORMMONITOR_H

#include "eventbus.h"
#include "basethread.h"
#include "platformdescription.h"
#include <thread>
#include <atomic>
#include <memory>
#include <string>
#include <log4cpp/Category.hh>

/*!
 * \class periodically samples information about the platform status.
 * This information is then encapsulated in a MonitorEvent and published
 * to the EventBus.
 * PlatformMonitor runs in a dedicated thread.
 */
class PlatformMonitor : public rmcommon::BaseThread {
    struct PlatformMonitorImpl;
    std::unique_ptr<PlatformMonitorImpl> pimpl_;
    log4cpp::Category &cat_;
    int monitorPeriod_;
    rmcommon::EventBus &bus_;

    virtual void run() override;
public:
    PlatformMonitor(rmcommon::EventBus &eventBus, PlatformDescription pd, int monitorPeriod);
    ~PlatformMonitor();

    /*!
     * \brief Sets the vector of possible CPU chip names.
     * \param names the comma separated list of module names
     */
    void setCpuModuleNames(const std::string &names);

    /*!
     * \brief Sets the vector of possible battery module names.
     * \param names the comma separated list of module names
     */
    void setBatteryModuleNames(const std::string &names);
};

#endif  // #ifndef PLATFORMMONITOR_H


