#ifndef ICPUSETCONTROL_H
#define ICPUSETCONTROL_H

#include "numericvalue.h"
#include "app.h"
#include "cpusetvector.h"

#include <vector>

namespace pc {

/*!
 * \interface an interface describing the behavior of a cpuset controller
 */
class ICpusetControl {
public:

    /*!
     * Requests the use of a set of processing units by the application.
     * \param cpus the vector of requested processing units
     * \param app the application to limit
     */
    virtual void setCpus(const rmcommon::CpusetVector &cpus, std::shared_ptr<rmcommon::App> app) = 0;

    /*!
     * Returns the list of processing units that are requested by the specified
     * application as a vector of pairs.
     * \param app the application of interest
     * \returns the processing units requested for use by the application
     */
    virtual rmcommon::CpusetVector getCpus(std::shared_ptr<rmcommon::App> app) = 0;

    /*!
     * Returns the list of processing units that are granted to the specified
     * application as a vector of pairs.
     * \param app the application of interest
     * \returns the processing units available for use by the application
     */
    virtual rmcommon::CpusetVector getCpusEffective(std::shared_ptr<rmcommon::App> app) = 0;

    /*!
     * Requests the use of a set of memory nodes by the application.
     * \param memNodes the list of requested memory nodes
     * \param app the application to limit
     */
    virtual void setMems(const rmcommon::CpusetVector &memNodes, std::shared_ptr<rmcommon::App> app) = 0;

    /*!
     * Returns the list of memory nodes that are requested by the specified application
     * as a vector of pairs.
     * \param app the application of interest
     * \returns the memory nodes requested for use by the application
     */
    virtual rmcommon::CpusetVector getMems(std::shared_ptr<rmcommon::App> app) = 0;

    /*!
     * Returns the list of memory nodes that are granted to the specified application
     * as a vector of pairs.
     * \param app the application of interest
     * \returns the memory nodes available for use by the application
     */
    virtual rmcommon::CpusetVector getMemsEffective(std::shared_ptr<rmcommon::App> app) = 0;

};

}
#endif // ICPUSETCONTROL_H
