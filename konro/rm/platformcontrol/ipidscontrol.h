#ifndef IPIDSCONTROL_H
#define IPIDSCONTROL_H

#include "numericvalue.h"
#include "app.h"

#include <vector>

namespace pc {

/*!
 * \interface an interface describing the behavior of a pids controller
 */
class IPidsControl {
public:

    /*!
     * Limits the number of processes that may be forked by the specified application.
     * To remove all limits, the caller must pass the string "max" as parameter.
     * \param numPids the maximum number of processes that can be forked
     * \param app the application to limit
     */
    virtual void setMax(rmcommon::NumericValue numPids, std::shared_ptr<rmcommon::App> app) = 0;

    /*!
     * Gets the maximum number of processes that may be forked by the application.
     * The function returns "max" if no upper bound is set.
     * \param app the application of interest
     * \returns the maximum number of processes that can be froked by the application
     */
    virtual rmcommon::NumericValue getMax(std::shared_ptr<rmcommon::App> app) = 0;

    /*!
     * Gets the number of processes that have been forked by the specified
     * application.
     * \param app the application of interest
     * \returns the number of processes forked by the app
     */
    virtual rmcommon::NumericValue getCurrent(std::shared_ptr<rmcommon::App> app) = 0;

};

}

#endif // IPIDSCONTROL_H
