#ifndef PLATFORMLOAD_H
#define PLATFORMLOAD_H

#include <vector>
#include <iostream>

namespace rmcommon {
/*!
 * \class encapsulates cpu utilization information
 */
class PlatformLoad {
    /*! Usage of each CPU */
    std::vector<int> cpus_;
    /*! Usage of each processing unit */
    std::vector<int> pus_;
public:
    void addCpuLoad(int load) {
        cpus_.push_back(load);
    }

    void addPULoad(int load) {
        pus_.push_back(load);
    }

    int getCpuLoad(int idx) {
        return cpus_[idx];
    }

    int getPULoad(int idx) {
        return pus_[idx];
    }

    const std::vector<int> &getPUs() {
        return pus_;
    }

    friend std::ostream &operator << (std::ostream &os, const PlatformLoad &pt) {
        bool first;
        os << '{'
           << "\"cpus\":";
        os << '[';
        first = true;
        for (const auto &cpu: pt.cpus_) {
            if (first)
                first = false;
            else
                os << ',';
            os << cpu;
        }
        os << ']';
        os << ",\"PUs\":";
        os << '[';
        first = true;
        for (const auto &pu: pt.pus_) {
            if (first)
                first = false;
            else
                os << ',';
            os << pu;
        }
        os << ']';
        os << '}';
        return os;
    }
};

}   // namespace rmcommon

#endif // PLATFORMLOAD_H
