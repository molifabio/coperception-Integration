#ifndef PLATFORMTEMPERATURE_H
#define PLATFORMTEMPERATURE_H

#include "componenttemperature.h"
#include <vector>
#include <iostream>

namespace rmcommon {
/*!
 * \class encapsulates thermal information about the machine
 */
class PlatformTemperature {
    std::vector<rmcommon::ComponentTemperature> cpus_;
    std::vector<rmcommon::ComponentTemperature> cores_;
public:
    void addCpuTemperature(const rmcommon::ComponentTemperature &ct) {
        cpus_.push_back(ct);
    }

    void addCoreTemperature(const rmcommon::ComponentTemperature &ct) {
        cores_.push_back(ct);
    }

    /*!
     * Returns temperature information about the CPU with the
     * specified OS index.
     */
    rmcommon::ComponentTemperature getCpuTemperature(int idx) {
        return cpus_[idx];
    }

    /*!
     * Returns temperature information about the core with the
     * specified OS index.
     */
    rmcommon::ComponentTemperature getCoreTemperature(int idx) {
        return cores_[idx];
    }

    /*!
     * Returns temperature information about all CPUs.
     */
     std::vector<rmcommon::ComponentTemperature> getCpusTemperature() {
        return cpus_;
    }

     /*!
      * Returns temperature information about all cores.
      */
      std::vector<rmcommon::ComponentTemperature> getCoresTemperature() {
         return cores_;
     }

    friend std::ostream &operator << (std::ostream &os, const PlatformTemperature &pt) {
        bool first;
        os << "{"
           << "\"cpus\":";
        os << "[";
        first = true;
        for (const auto &cpu: pt.cpus_) {
            if (first)
                first = false;
            else
                os << ',';
            os << cpu;
        }
        os << "]";
        os << ",\"cores\":";
        os << "[";
        first = true;
        for (const auto &core: pt.cores_) {
            if (first)
                first = false;
            else
                os << ',';
            os << core;
        }
        os << "]";
        os << "}";
        return os;
    }
};

}   // namespace rmcommon

#endif // PLATFORMTEMPERATURE_H
