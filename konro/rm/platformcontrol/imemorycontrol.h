#ifndef IMEMORYCONTROL_H
#define IMEMORYCONTROL_H

#include "numericvalue.h"
#include "app.h"

namespace pc {

/*!
 * \interface an interface describing the behavior of a memory controller
 */
class IMemoryControl {
public:

    /*!
     * Gets the total amount of memory currently being used by the specified
     * application and its descendants.
     * \returns the amount of memory used by the app and its descendants
     */
    virtual int getCurrent(std::shared_ptr<rmcommon::App> app) = 0;

    /*!
     * Sets a minimum amount of memory that the application must always retain.
     * To remove a limit, minMem must be set to 0.
     * \param minMem the min amount of memory that the app must retain.
     * \param app the application to limit
     */
    virtual void setMin(int minMem, std::shared_ptr<rmcommon::App> app) = 0;

    /*!
     * Gets the minimum amount of memory that the application must always retain.
     * The default value is 0.
     * \param app the application of interest
     * \returns the min amount of memory that the app must retain
     */
    virtual int getMin(std::shared_ptr<rmcommon::App> app) = 0;

    /*!
     * Sets a memory usage hard limit for the application.
     * To remove a limit, maxMem must be set to "max".
     * \param maxMem the max amount of memory that the app can use.
     * \param app the application to limit
     */
    virtual void setMax(rmcommon::NumericValue maxMem, std::shared_ptr<rmcommon::App> app) = 0;

    /*!
     * Gets the memory usage hard limit for the application.
     * The default value is "max".
     * \param app the application of interest
     * \returns the max amount of memory that the app can use
     */
    virtual rmcommon::NumericValue getMax(std::shared_ptr<rmcommon::App> app) = 0;

};

}
#endif // IMEMORYCONTROL_H
