#ifndef CPUSETVECTOR_H
#define CPUSETVECTOR_H

#include <vector>
#include <utility>
#include <set>
#include <string>

namespace rmcommon {

/* Vector of cpu numbers pairs */
typedef std::vector<std::pair<short,short>> CpusetVector;

/*!
 * Returns the number of PUs listed in the vector
 */
int countPUs(const CpusetVector &vec);
bool containsPU(const CpusetVector &vec, short pu);

/*!
 * Converts the CpusetVector to a set of PU numbers
 */
std::set<short> toSet(const CpusetVector &vec);

/*!
 * Converts a set of PU numbers to a CpusetVector
 */
CpusetVector toCpusetVector(const std::set<short> &puSet);

/*!
 * Converts a set of PU numbers to a linear vector of PUs
 */
std::vector<short> toVector(const std::set<short> &puSet);

bool removePU(CpusetVector &vec, short pu);

bool addPU(CpusetVector &vec, short pu);

/*!
 * Converts a CpusetVector to a linear vector of PUs
 */
std::vector<short> toVector(const CpusetVector &vec);

CpusetVector toCpusetVector(std::vector<short> puVec);

std::string toString(const CpusetVector &vec);

}   // namespace rmcommon

#endif // CPUSETVECTOR_H
