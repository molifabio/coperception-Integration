#ifndef NAMESPACES_H
#define NAMESPACES_H

#include <sys/types.h>

namespace rmcommon {

typedef unsigned long namespace_t;

/*!
 * Returns the pid namespace of the caller process
 */
namespace_t getSelfPidNamespace();

/*!
 * Gets the pid namespace of the specified process
 */
namespace_t getPidNamespace(pid_t pid);

/*!
 * Returns the PID in the current namespace of the pid
 * in namespace "ns"
 *
 * \param pid the PID in namespace "ns"
 * \param ns the namespace to which "pid" belongs
 * \return the target process PID in Konro's namespace
 */
pid_t mapPid(pid_t pid, namespace_t ns);

/*!
 * \brief Checks if the specified container is wrapped by a Kubernetes pod
 * \param pid the pid of the container
 * \return true if the container is inside a pod, false otherwise
 */
bool isKubPod(pid_t pid);

}   // namespace rmcommon

#endif // NAMESPACES_H
