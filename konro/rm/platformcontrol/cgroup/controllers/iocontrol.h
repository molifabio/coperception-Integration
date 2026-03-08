#ifndef IOCONTROL_H
#define IOCONTROL_H

#include "numericvalue.h"
#include "../cgroupcontrol.h"
#include "../iiocontrol.h"
#include <string>
#include <map>

namespace pc {
/*!
 * \class a class for interacting with the cgroup IO controller
 */
class IOControl : public IIoControl, CGroupControl {
public:
    const int MAX_IO_CONTROL = -1;

    enum ControllerFile {
        STAT,   // read-only
        MAX     // read-write
    };


private:
    static const char *controllerName_;
    static const std::map<ControllerFile, const char *> fileNamesMap_;
    // IO resources that can be constrained
    static const std::map<IoMax, const char *> keyNames_;

    /*!
     * Helper function returning a map of key-values for the specified IO device and controller
     * interface file.
     * \param cf the controller interface file
     * \param major the device major number
     * \param minor the device minor number
     * \param app the application of interest
     */
    std::map<std::string, rmcommon::NumericValue> getIOHelper(ControllerFile cf, int major, int minor, std::shared_ptr<rmcommon::App> app);

    IOControl() = default;

public:

    static IOControl &instance();

    /*!
     * Gets the IO usage statistics for the specified applications.
     *
     * Statistics are available as a set of key-value pairs.
     * Examples of available statistics:
     * rbytes: bytes read
     * wios: number of write IOs
     *
     * \param major the device major number
     * \param minor the device minor number
     * \param app the application of interest
     * \returns the IO usage statistics
     */
    std::map<std::string, rmcommon::NumericValue> getStat(int major, int minor, std::shared_ptr<rmcommon::App> app) {
        return getIOHelper(STAT, major, minor, app);
    }

    /*!
     * Sets an IO limit for the specified device and application.
     *
     * The device number is specified through a <major, minor> pair.
     * The IO resource to constraint is specified through a member of the IoMax struct.
     * To remove a limit, the caller must pass the string "max" as value argument.
     *
     * \example setIOMax(8, 16, WIOPS, 120, app1)
     *          Sets a write limit at 120 IOPS for device 8:16.
     *
     * \param major the device major number
     * \param minor the device minor number
     * \param ioMax the IO resource to limit
     * \param value the maximum value allowed for the IO resource
     * \param app the application to limit
     */
    void setMax(int major, int minor, IoMax ioMax, rmcommon::NumericValue value, std::shared_ptr<rmcommon::App> app) override;

    /*!
     * Gets the specified application's IO limits.
     *
     * Statistics are available as a set of key-value pairs.
     * Examples of available statistics:
     * usage_usec: total cpu time
     * user_usec: user cpu time
     *
     * \param app the application of interest
     * \returns the cpu time statistics
     */
    std::map<std::string, rmcommon::NumericValue> getMax(int major, int minor, std::shared_ptr<rmcommon::App> app) override {
        return getIOHelper(MAX, major, minor, app);
    }
};

}   // namespace pc
#endif // IOCONTROL_H
