#ifndef IIOCONTROL_H
#define IIOCONTROL_H

#include "numericvalue.h"
#include "app.h"

#include <map>

namespace pc {

/*!
 * \interface an interface describing the behavior of an IO controller
 */
class IIoControl {
public:

    /* IO resources that can be constrained */
    enum IoMax {
        RBPS,   // Max read bytes per second
        WBPS,   // Max write bytes per second
        RIOPS,  // Max read IO operations per second
        WIOPS   // Max write IO operations per second
    };

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
    virtual void setMax(int major, int minor, IoMax ioMax, rmcommon::NumericValue value, std::shared_ptr<rmcommon::App> app) = 0;

    /*!
     * Gets the specified application's IO limits.
     * \param app the application of interest
     * \returns the cpu time statistics
     */
    virtual std::map<std::string, rmcommon::NumericValue> getMax(int major, int minor, std::shared_ptr<rmcommon::App> app) = 0;

};

}

#endif // IIOCONTROL_H
