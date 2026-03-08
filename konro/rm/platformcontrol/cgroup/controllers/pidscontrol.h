#ifndef PIDSCONTROL_H
#define PIDSCONTROL_H

#include "numericvalue.h"
#include "../cgroupcontrol.h"
#include "../ipidscontrol.h"
#include <string>
#include <map>

namespace pc {
/*!
 * \class a class for interacting with the cgroup PIDs controller
 */
class PidsControl : public IPidsControl, CGroupControl {
public:
    enum ControllerFile {
        MAX,        // read-write
        CURRENT     // read-only
    };
    const int MAX_NUM_PIDS = -1;
private:
    static const char *controllerName_;
    static const std::map<ControllerFile, const char *> fileNamesMap_;

    PidsControl() = default;

public:

    static PidsControl &instance();

    /*!
     * Limits the number of processes that may be forked by the specified application.
     * To remove all limits, the caller must pass the string "max" as parameter.
     * \param numPids the maximum number of processes that can be forked
     * \param app the application to limit
     */
    void setMax(rmcommon::NumericValue numPids, std::shared_ptr<rmcommon::App> app) override;

    /*!
     * Gets the maximum number of processes that may be forked by the application.
     * The function returns "max" if no upper bound is set.
     * \param app the application of interest
     * \returns the maximum number of processes that can be froked by the application
     */
    rmcommon::NumericValue getMax(std::shared_ptr<rmcommon::App> app) override;

    /*!
     * Gets the number of processes in the cgroup where the application is located and
     * all its descendants.
     * \param app the application of interest
     * \returns the number of processes in the app's cgroup and descendants
     */
    rmcommon::NumericValue getCurrent(std::shared_ptr<rmcommon::App> app) override;
};

}   // namespace pc
#endif // PIDSCONTROL_H
