#ifndef CPUSETCONTROL_H
#define CPUSETCONTROL_H

#include "../cgroupcontrol.h"
#include "../icpusetcontrol.h"
#include <string>
#include <map>
#include <vector>
#include <utility>

namespace pc {
/*!
 * \class a class for interacting with the cgroup cpuset controller
 */
class CpusetControl : public ICpusetControl, CGroupControl {
public:
    enum ControllerFile {
        CPUS,               // read-write
        CPUS_EFFECTIVE,     // read-only
        MEMS,               // read-write
        MEMS_EFFECTIVE      // read-only
    };
private:
    static const char *controllerName_;
    static const std::map<ControllerFile, const char *> fileNamesMap_;

    CpusetControl() = default;
public:

    static CpusetControl &instance();

    /*!
     * Parses the content of a cpuset interface file and converts it to
     * a vector of pairs.
     *
     * \example "1, 2-3" is converted to {1,1}, {2,3}
     *
     * \param line the content of the cpuset interface file
     * \returns the equivalent vector of pairs
     */
    rmcommon::CpusetVector parseCpuSet(const std::string &line);

    /*!
     * Requests the use of a set of cpus by the application.
     *
     * The cpu numbers are expressed through a vector of pairs. A pair with equal
     * numbers, such as {1,1}, represents a single cpu (e.g. CPU 1). A pair with
     * different numbers, such as {0,2} represents a range of cpu numbers
     * (e.g. CPUs 0,1 and 2).
     * An empty vector indicates that the cgroup is using the same setting as the
     * nearest cgroup ancestor with a non-empty cpuset.cpus or all the available
     * cpus if none is found.
     *
     * \example {0, 0}, {2, 3} represents CPUs 0, 2 and 3
     *
     * \param cpus the vector of requested cpus
     * \param app the application to limit
     */
    void setCpus(const rmcommon::CpusetVector &cpus, std::shared_ptr<rmcommon::App> app) override;

    /*!
     * Returns the list of cpus that are requested by the specified application
     * as a vector of pairs.
     *
     * If no request for CPU sets has been issued, the function will
     * return an empty vector.
     *
     * \param app the application of interest
     * \returns the cpus requested for use by the application
     */
    rmcommon::CpusetVector getCpus(std::shared_ptr<rmcommon::App> app) override;

    /*!
     * Returns the list of cpus that are granted to the specified application
     * as a vector of pairs.
     * \param app the application of interest
     * \returns the cpus available for use by the application
     */
    rmcommon::CpusetVector getCpusEffective(std::shared_ptr<rmcommon::App> app) override;

    /*!
     * Requests the use of a set of memory nodes by the application.
     *
     * The memory node numbers are expressed through a vector of pairs. A pair with
     * equal numbers, such as {1,1}, represents a single memory node (e.g. node 1).
     * A pair with different numbers, such as {0,2} represents a range of nodes
     * (e.g. nodes 0,1 and 2).
     * An empty vector indicates that the cgroup is using the same setting as the
     * nearest cgroup ancestor with a non-empty cpuset.mems or all the available
     * cpus if none is found.
     *
     * \example {0, 0}, {2, 3} represents memory nodes 0, 2 and 3
     *
     * \note Setting a non-empty value causes memory of tasks within
     * the cgroup to be migrated to the designated nodes if they are currently using
     * memory outside of the designated nodes.
     *
     * \param memNodes the list of requested memory nodes
     * \param app the application to limit
     */
    void setMems(const rmcommon::CpusetVector &memNodes, std::shared_ptr<rmcommon::App> app) override;

    /*!
     * Returns the list of memory nodes that are requested by the specified application
     * as a vector of pairs.
     *
     * If no request for memory nodes has been issued, the function will
     * return an empty vector.
     *
     * \param app the application of interest
     * \returns the memory nodes requested for use by the application
     */
    rmcommon::CpusetVector getMems(std::shared_ptr<rmcommon::App> app) override;

    /*!
     * Returns the list of memory nodes that are granted to the specified application
     * as a vector of pairs.
     * \param app the application of interest
     * \returns the memory nodes available for use by the application
     */
    rmcommon::CpusetVector getMemsEffective(std::shared_ptr<rmcommon::App> app) override;

};

}   // namespace pc
#endif // CPUSETCONTROL_H
