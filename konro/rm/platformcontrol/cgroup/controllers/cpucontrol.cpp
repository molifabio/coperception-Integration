#include "../cgroupcontrol.h"
#include "cpucontrol.h"
#include <cstdlib>

namespace pc {

/*static*/
const char *CpuControl::controllerName_ = "cpu";

/*static*/
const std::map<CpuControl::ControllerFile, const char *> CpuControl::fileNamesMap_ = {
    { WEIGHT, "cpu.weight" },
    { MAX, "cpu.max" },
    { STAT, "cpu.stat" }
};


/*static*/
CpuControl &CpuControl::instance()
{
    static CpuControl cc;
    return cc;
}

void CpuControl::setMax(rmcommon::NumericValue percentage, std::shared_ptr<rmcommon::App> app)
{
    std::ostringstream os;
    // for cpu.max normalize value between 0 and period_
    if (percentage.isMax()) {
        os << percentage << ' ' << period_;
    } else {
        os << ((percentage * period_) / 100) << ' ' << period_;
    }
    setValue(controllerName_, fileNamesMap_.at(MAX), os.str(), app);
}

rmcommon::NumericValue CpuControl::getMax(std::shared_ptr<rmcommon::App> app)
{
    std::string svalue = getLine(controllerName_, fileNamesMap_.at(MAX), app);
    std::istringstream is(svalue);
    rmcommon::NumericValue value;
    int period;
    is >> value >> period;
    if (is.fail()) {
        return rmcommon::NumericValue();      // return invalid numeric value
    } else if (value.isInvalid() || value.isMax()) {
        return value;
    } else {
        // return normalized value
        return (value * 100) / period;
    }
}

std::map<std::string, uint64_t> CpuControl::getStat(std::shared_ptr<rmcommon::App> app)
{
    return getContentAsMap(controllerName_, fileNamesMap_.at(STAT), app);
}

void CpuControl::setWeight(int weight, std::shared_ptr<rmcommon::App> app)
{
    setValue(controllerName_, fileNamesMap_.at(WEIGHT), weight, app);
}


}   // namespace pc
