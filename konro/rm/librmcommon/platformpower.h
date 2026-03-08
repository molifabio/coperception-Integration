#ifndef PLATFORMPOWER_H
#define PLATFORMPOWER_H

#include <iostream>

namespace rmcommon {
/*!
 * \class encapsulates power information about the machine
 */
class PlatformPower {
    /* Current value in mA */
    int batteryCurrent_;
    /* Voltage value in V */
    int batteryVoltage_;
public:

    void setBatteryCurrent(int val) {
        batteryCurrent_ = val;
    }

    void setBatteryVoltage(int val) {
        batteryVoltage_ = val;
    }

    int getBatteryCurrent() {
        return batteryCurrent_;
    }

    int getBatteryVoltage() {
        return batteryVoltage_;
    }

    friend std::ostream &operator << (std::ostream &os, const PlatformPower &pp) {
        os << "{";
        os << "\"batteryCurrent\":" << pp.batteryCurrent_;
        os << ",\"batteryVoltage\":" << pp.batteryVoltage_;
        os << "}";
        return os;
    }
};

}   // namespace rmcommon

#endif // PLATFORMPOWER_H
