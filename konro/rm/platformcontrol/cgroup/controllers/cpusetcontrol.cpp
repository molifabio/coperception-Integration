#include "cpusetcontrol.h"
#include "../pcexception.h"
#include <sstream>
#include <cctype>
#include <cstdlib>

namespace pc {

/*static*/
const char *CpusetControl::controllerName_ = "cpuset";

/*static*/
const std::map<CpusetControl::ControllerFile, const char *> CpusetControl::fileNamesMap_ = {
    { CPUS, "cpuset.cpus" },
    { CPUS_EFFECTIVE, "cpuset.cpus.effective" },
    { MEMS, "cpuset.mems" },
    { MEMS_EFFECTIVE, "cpuset.mems.effective" }
};

CpusetControl &CpusetControl::instance()
{
    static CpusetControl cc;
    return cc;
}

std::vector<std::pair<short, short>> CpusetControl::parseCpuSet(const std::string &line)
{
    std::vector<std::pair<short, short>> vec;
    const char *p = line.c_str();
    while (*p) {
        if (!isdigit(*p))
            throw PcException("getCpusetCpus: invalid format");
        const char *endptr;
        int cpu1 = (int)strtol(p, (char **)&endptr, 10);
        if (*endptr == '\0') {
            vec.emplace_back(cpu1, cpu1);
            p = endptr;
        } else if (*endptr == ',') {
            vec.emplace_back(cpu1, cpu1);
            p = endptr + 1;
        } else if (*endptr == '-') {
            p = endptr + 1;
            if (!isdigit(*p))
                throw PcException("getCpusetCpus: invalid format");
            int cpu2 = (int)strtol(p, (char **)&endptr, 10);
            vec.emplace_back(cpu1, cpu2);
            if (*endptr == '\0')
                p = endptr;
            else if (*endptr == ',')
                p = endptr + 1;
            else
                throw PcException("getCpusetCpus: invalid format");
        } else {
            throw PcException("getCpusetCpus: invalid format");
        }
    }
    return vec;
}

void CpusetControl::setCpus(const std::vector<std::pair<short, short>> &cpus, std::shared_ptr<rmcommon::App> app)
{
    std::ostringstream os;
    for (size_t i = 0; i < cpus.size(); ++i) {
        if (i > 0)
            os << ',';
        os << cpus[i].first;
        if (cpus[i].second != cpus[i].first)
            os << '-' << cpus[i].second;
    }
    setValue(controllerName_, fileNamesMap_.at(CPUS), os.str(), app);
}

std::vector<std::pair<short, short>> CpusetControl::getCpus(std::shared_ptr<rmcommon::App> app)
{
    std::string line = getLine(controllerName_, fileNamesMap_.at(CPUS), app);
    return parseCpuSet(line);
}

std::vector<std::pair<short, short>> CpusetControl::getCpusEffective(std::shared_ptr<rmcommon::App> app)
{
    std::string line = getLine(controllerName_, fileNamesMap_.at(CPUS_EFFECTIVE), app);
    return parseCpuSet(line);
}

void CpusetControl::setMems(const std::vector<std::pair<short, short> > &memNodes, std::shared_ptr<rmcommon::App> app)
{
    std::ostringstream os;
    for (size_t i = 0; i < memNodes.size(); ++i) {
        if (i > 0)
            os << ',';
        os << memNodes[i].first;
        if (memNodes[i].second != memNodes[i].first)
            os << '-' << memNodes[i].second;
    }
    setValue(controllerName_, fileNamesMap_.at(MEMS), os.str(), app);
}

std::vector<std::pair<short, short> > CpusetControl::getMems(std::shared_ptr<rmcommon::App> app)
{
    std::string line = getLine(controllerName_, fileNamesMap_.at(MEMS), app);
    return parseCpuSet(line);
}

std::vector<std::pair<short, short> > CpusetControl::getMemsEffective(std::shared_ptr<rmcommon::App> app)
{
    std::string line = getLine(controllerName_, fileNamesMap_.at(MEMS_EFFECTIVE), app);
    return parseCpuSet(line);
}

}   // namespace pc
