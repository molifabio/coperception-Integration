#ifndef ICPUCONTROL_H
#define ICPUCONTROL_H

#include "numericvalue.h"
#include "app.h"

namespace pc {

/*!
 * \interface an interface describing the behavior of a CPU controller
 */
class ICpuControl {
public:

    /*!
     * Sets a maximum cpu bandwidth limit for the specified application.
     * \param percentage the maximum percentage of cpu utilization allowed
     * \param app the application to limit
     */
    virtual void setMax(rmcommon::NumericValue percentage, std::shared_ptr<rmcommon::App> app) = 0;

    /*!
     * Gets the cpu bandwidth limit set for the specified application.
     * \param app the application of interest
     * \returns the maximum percentage of cpu utilization allowed for the app
     */
    virtual rmcommon::NumericValue getMax(std::shared_ptr<rmcommon::App> app) = 0;

};

}
#endif // ICPUCONTROL_H
