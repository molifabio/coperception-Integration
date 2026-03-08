#include "memorycontrol.h"

namespace pc {

/*static*/
const char *MemoryControl::controllerName_ = "memory";

/*static*/
const std::map<MemoryControl::ControllerFile, const char *> MemoryControl::fileNamesMap_ = {
    { CURRENT, "memory.current" },
    { MIN, "memory.min" },
    { MAX, "memory.max" },
    { EVENTS, "memory.events" },
    { STAT, "memory.stat" }
};

MemoryControl &MemoryControl::instance()
{
    static MemoryControl mc;
    return mc;
}

int MemoryControl::getCurrent(std::shared_ptr<rmcommon::App> app)
{
    return getValueAsInt(controllerName_, fileNamesMap_.at(CURRENT), app);
}

void MemoryControl::setMin(int minMem, std::shared_ptr<rmcommon::App> app)
{
    setValue(controllerName_, fileNamesMap_.at(MIN), minMem, app);
}

int MemoryControl::getMin(std::shared_ptr<rmcommon::App> app)
{
    return getValueAsInt(controllerName_, fileNamesMap_.at(MIN), app);
}

void MemoryControl::setMax(rmcommon::NumericValue maxMem, std::shared_ptr<rmcommon::App> app)
{
    setValue(controllerName_, fileNamesMap_.at(MAX), maxMem, app);
}

rmcommon::NumericValue MemoryControl::getMax(std::shared_ptr<rmcommon::App> app)
{
    return getLine(controllerName_, fileNamesMap_.at(MAX), app);
}

std::map<std::string, uint64_t> MemoryControl::getEvents(std::shared_ptr<rmcommon::App> app)
{
    return getContentAsMap(controllerName_, fileNamesMap_.at(EVENTS), app);
}

std::map<std::string, uint64_t> MemoryControl::getStat(std::shared_ptr<rmcommon::App> app)
{
    return getContentAsMap(controllerName_, fileNamesMap_.at(STAT), app);
}

}   // namespace pc
