#ifndef COMPONENTTEMPERATURE_H
#define COMPONENTTEMPERATURE_H

#include <string>
#include <iostream>

namespace rmcommon {

/*!
 * \brief encapsulates temperature information about a machine component.
 * A component can be either a Package or a CPU.
 */
struct ComponentTemperature {
    int num_;
    char *label_;
    int temp_;
    int maxTemp_;
    int minTemp_;
    int highestTemp_;
    int lowestTemp_;
    int critTemp_;
    int emergencyTemp_;

    explicit ComponentTemperature() {
        label_ = nullptr;
        num_ = temp_ = maxTemp_ = minTemp_ = highestTemp_ = lowestTemp_ = critTemp_ = emergencyTemp_ =-1;
    }

    friend std::ostream &operator << (std::ostream &os, const ComponentTemperature &ct) {
        os << "{"
           << "\"num\":" << ct.num_
           << ",\"label\":" << '"' << ct.label_ << '"'
           << ",\"temp\":" << ct.temp_
           << ",\"maxTemp\":" << ct.maxTemp_
           << ",\"critTemp\":" << ct.critTemp_
           <<"}";
        return os;
    }
};

}   // namespace rmcommon

#endif // COMPONENTTEMPERATURE_H
