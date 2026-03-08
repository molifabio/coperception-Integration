#ifndef CPUCONTROL_H
#define CPUCONTROL_H

#include "numericvalue.h"
#include "../cgroupcontrol.h"
#include "../icpucontrol.h"
#include <string>
#include <map>
#include <iostream>
#include <sstream>


namespace pc {
/*!
 * \class a class for interacting with the cgroup cpu controller
 */
class CpuControl : public ICpuControl, CGroupControl {
    // the default period duration for each process
    const int period_ = 100000;
public:
    enum ControllerFile {
        WEIGHT,         // read-write
        MAX,            // read-write
        STAT            // read-only
    };
private:
    static const char *controllerName_;
    static const std::map<ControllerFile, const char *> fileNamesMap_;

    CpuControl() = default;
public:

    static CpuControl &instance();
    /*!
     * Sets a maximum cpu bandwidth limit for the specified application.
     * \param percentage the maximum percentage of cpu utilization allowed
     * \param app the application to limit
     */
    void setMax(rmcommon::NumericValue percentage, std::shared_ptr<rmcommon::App> app) override;

    /*!
     * Gets the cpu bandwidth limit set for the specified application.
     * \param app the application of interest
     * \returns the maximum percentage of cpu utilization allowed for the app
     */
    rmcommon::NumericValue getMax(std::shared_ptr<rmcommon::App> app) override;

    /*!
     * Gets the cpu time statistics for the specified application.
     *
     * Statistics are available as a set of key-value pairs.
     * Examples of available statistics:
     * usage_usec: total cpu time
     * user_usec: user cpu time
     *
     * \param app the application of interest
     * \returns the cpu time statistics
     */
    std::map<std::string, uint64_t> getStat(std::shared_ptr<rmcommon::App> app);

    /*!
     * Sets a proportional cpu bandwidth limit for the specified application.
     *
     * A parent’s resource is distributed by adding up the weights of all active
     * children and giving each the fraction matching the ratio of its weight
     * against the sum.
     *
     * \example Given 10 cgroups, each with weight of value 100, the sum is 1000
     *          and each cgroup receives one tenth of the resource.
     *
     * \param weight the share of total cpu resources held by the app
     * \param app the application to limit
     */
    void setWeight(int weight, std::shared_ptr<rmcommon::App> app);

    /*!
     * Gets the fraction of cpu time assigned to the specified application.
     * \param app the application of interest
     * \returns the fraction of cpu time as a int in the range [0,1]
     */
    int getWeight(std::shared_ptr<rmcommon::App> app);

};

}   // namespace pc
#endif // CPUCONTROL_H
