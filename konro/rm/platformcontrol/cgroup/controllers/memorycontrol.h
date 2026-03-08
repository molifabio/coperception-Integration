#ifndef MEMORYCONTROL_H
#define MEMORYCONTROL_H

#include "numericvalue.h"
#include "../cgroupcontrol.h"
#include "../imemorycontrol.h"
#include <string>
#include <map>

namespace pc {
/*!
 * \class a class for interacting with the cgroup memory controller.
 * All memory amounts are in bytes.
 */
class MemoryControl : public IMemoryControl, CGroupControl {
public:
    enum ControllerFile {
        CURRENT,    // read-only
        MIN,        // read-write
        MAX,        // read-write
        EVENTS,     // read-only
        STAT        // read-only
    };
private:
    static const char *controllerName_;
    static const std::map<ControllerFile, const char *> fileNamesMap_;

    MemoryControl() = default;

public:

    static MemoryControl &instance();

    /*!
     * Gets the total amount of memory currently being used by the specified
     * application and its descendants.
     * For example:
     * \code
     * 2571833344
     * \endcode
     *
     * \returns the amount of memory used by the app and its descendants
     */
    int getCurrent(std::shared_ptr<rmcommon::App> app) override;

    /*!
     * Sets a minimum amount of memory that the application must always retain.
     * It is a hard memory protection.
     * To remove a limit, minMem must be set to 0.
     * \param minMem the min amount of memory that the app must retain.
     *               minMem must be a multiple of the page size (for example: 4096)
     *               or it will be rounded
     * \param app the application to limit
     */
    void setMin(int minMem, std::shared_ptr<rmcommon::App> app) override;

    /*!
     * Gets the minimum amount of memory that the application must always retain.
     * The default value is 0.
     * \param app the application of interest
     * \returns the min amount of memory that the app must retain
     */
    int getMin(std::shared_ptr<rmcommon::App> app) override;

    /*!
     * Sets a memory usage hard limit for the application.
     * If the app's memory usage reaches this limit and can't be reduced,
     * the system OOM killer is invoked on the app.
     * To remove a limit, maxMem must be set to "max".
     * \param maxMem the max amount of memory that the app can use.
     *        maxMem must be a multiple of the page size (for example: 4096)
     *        or it will be rounded
     * \param app the application to limit
     */
    void setMax(rmcommon::NumericValue maxMem, std::shared_ptr<rmcommon::App> app) override;

    /*!
     * Gets the memory usage hard limit for the application.
     * The default value is "max".
     * \param app the application of interest
     * \returns the max amount of memory that the app can use
     */
    rmcommon::NumericValue getMax(std::shared_ptr<rmcommon::App> app) override;

    /*!
     * Gets the number of times certain memory events have occurred
     * for the specified application.
     *
     * Events are available as a set of key-value pairs.
     * Examples of available events:
     * max: number of times memory usage was about to go over the max boundary
     * oom_kill: number of processes belonging to this cgroup killed by the OOM killer
     *
     * \param app the application of interest
     * \returns the pairs of memory events
     */
    std::map<std::string, uint64_t> getEvents(std::shared_ptr<rmcommon::App> app);

    /*!
     * Gets memory statistics for the specified applications.
     *
     * Statistics are available as a set of key-value pairs.
     * Examples of available statistics:
     * kernel_stack: amount of memory allocated to kernel stacks
     * pagetables: amount of memory allocated for page tables
     *
     * \param app the application of interest
     * \returns the memory statistics
     */
    std::map<std::string, uint64_t> getStat(std::shared_ptr<rmcommon::App> app);

};

}   // namespace pc

#endif // MEMORYCONTROL_H
