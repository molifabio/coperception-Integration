#include "pidscontrol.h"
#include <cstdlib>

namespace pc {

/*static*/
const char *PidsControl::controllerName_ = "pids";

/*static*/
const std::map<PidsControl::ControllerFile, const char *> PidsControl::fileNamesMap_ = {
    { MAX, "pids.max" },
    { CURRENT, "pids.current" }
};

PidsControl &PidsControl::instance()
{
    static PidsControl pc;
    return pc;
}

void PidsControl::setMax(rmcommon::NumericValue numPids, std::shared_ptr<rmcommon::App> app)
{
    setValue(controllerName_, fileNamesMap_.at(MAX), numPids, app);
}

rmcommon::NumericValue PidsControl::getMax(std::shared_ptr<rmcommon::App> app)
{
    return getLine(controllerName_, fileNamesMap_.at(MAX), app);
}

rmcommon::NumericValue PidsControl::getCurrent(std::shared_ptr<rmcommon::App> app)
{
    return getValueAsInt(controllerName_, fileNamesMap_.at(CURRENT), app);
}

}   // namespace pc
