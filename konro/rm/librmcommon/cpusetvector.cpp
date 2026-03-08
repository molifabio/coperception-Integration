#include "cpusetvector.h"
#include <numeric>
#include <sstream>
#include <algorithm>

namespace rmcommon {

int countPUs(const CpusetVector &vec)
{
    int n = 0;
    return std::accumulate(vec.begin(), vec.end(), n,
                    [](short init, const std::pair<short,short> &p) {
                        return init + (p.second - p.first + 1);
    });
}

bool containsPU(const CpusetVector &vec, short pu)
{
    for (const std::pair<short,short> &p: vec) {
        if (pu >= p.first && pu <= p.second) {
            return true;
        }
    }
    return false;
}

std::set<short> toSet(const CpusetVector &vec)
{
    std::set<short> res;
    for (auto &c : vec) {
        for (int i = c.first; i <= c.second; ++i) {
            res.insert(i);
        }
    }
    return res;
}

CpusetVector toCpusetVector(const std::set<short> &puSet)
{
    short lastPu = -10;
    rmcommon::CpusetVector res;
    for (auto &pu : puSet) {
        if (pu == lastPu + 1) {
            res[res.size()-1].second = pu;
        } else {
            res.push_back({pu, pu});
        }
        lastPu = pu;
        // lastPu is always equal to res[res.size()-1].second
    }
    return res;
}

std::vector<short> toVector(const std::set<short> &puSet)
{
    std::vector<short> vec(puSet.begin(), puSet.end());
    return vec;
}

std::vector<short> toVector(const CpusetVector &vec)
{
    std::vector<short> res;
    for (auto &c : vec) {
        for (int i = c.first; i <= c.second; ++i) {
            res.push_back(i);
        }
    }
    return res;
}

CpusetVector toCpusetVector(std::vector<short> puVec)
{
    std::sort(puVec.begin(), puVec.end());
    short lastPu = -10;
    rmcommon::CpusetVector res;
    for (auto &pu : puVec) {
        if (pu == lastPu + 1) {
            res[res.size()-1].second = pu;
        } else {
            res.push_back({pu, pu});
        }
        lastPu = pu;
        // lastPu is always equal to res[res.size()-1].second
    }
    return res;
}

bool removePU(CpusetVector &vec, short pu)
{
    if (pu == -1)
        return false;
    std::set<short> puSet = rmcommon::toSet(vec);
    puSet.erase(pu);
    vec = rmcommon::toCpusetVector(puSet);
    return true;
}

bool addPU(CpusetVector &vec, short pu)
{
    if (pu == -1)
        return false;
    std::set<short> puSet = rmcommon::toSet(vec);
    puSet.insert(pu);
    vec = rmcommon::toCpusetVector(puSet);
    return true;
}

std::string toString(const CpusetVector &vec)
{
    std::ostringstream os;

    bool f = true;
    for (const auto &p: vec) {
        if (f) {
            f = false;
        } else {
            os << ',';
        }
        if (p.first == p.second) {
            os << p.first;
        } else {
            os << p.first << '-' << p.second;
        }
    }
    return os.str();
}

}   // namespace rmcommon
