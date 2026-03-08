#include "unittest.h"
#include "cgroup/controllers/cpusetcontrol.h"
#include "pcexception.h"

#include <vector>
#include <string>
#include <utility>
#include <iostream>
#include <iomanip>
#include <map>
#include <algorithm>

using namespace std;
using namespace pc;

static int testParseCpuSet(const char *line)
{
    try {
        vector<std::pair<short, short>> cpus = CpusetControl::instance().parseCpuSet(line);
    } catch (PcException &e) {
        return TEST_FAILED;
    }
    return TEST_OK;
}

static int testParseCpuSetFail(const char *line)
{
    try {
        vector<std::pair<short, short>> cpus = CpusetControl::instance().parseCpuSet(line);
    } catch (PcException &e) {
        return TEST_OK;
    }
    return TEST_FAILED;
}

static int testParseCpusetValues1()
{
    const char *line = "0-1,3,4-6,5";
    try {
        vector<pair<short, short>> cpus = CpusetControl::instance().parseCpuSet(line);
        vector<pair<short, short>>::iterator it;
        it = find(begin(cpus), end(cpus), make_pair((short)0, (short)1));
        if (it == cpus.end())
            return TEST_FAILED;
        it = find(begin(cpus), end(cpus), make_pair((short)3, (short)3));
        if (it == cpus.end())
            return TEST_FAILED;
        it = find(begin(cpus), end(cpus), make_pair((short)4, (short)6));
        if (it == cpus.end())
            return TEST_FAILED;
        it = find(begin(cpus), end(cpus), make_pair((short)5, (short)5));
        if (it == cpus.end())
            return TEST_FAILED;
    } catch (PcException &e) {
        return TEST_FAILED;
    }

    return TEST_OK;
}

int main(int argc, char *argv[])
{
    if (testParseCpuSet("") != TEST_OK)
        return TEST_FAILED;
    if (testParseCpuSet("1") != TEST_OK)
        return TEST_FAILED;
    if (testParseCpuSet("1,2") != TEST_OK)
        return TEST_FAILED;
    if (testParseCpuSet("1,2,3") != TEST_OK)
        return TEST_FAILED;
    if (testParseCpuSet("1-3") != TEST_OK)
        return TEST_FAILED;
    if (testParseCpuSet("0-1,2,3-4") != TEST_OK)
        return TEST_FAILED;

    if (testParseCpuSetFail(" ") != TEST_OK)
        return TEST_FAILED;
    if (testParseCpuSetFail("1 ") != TEST_OK)
        return TEST_FAILED;
    if (testParseCpuSetFail("1 2") != TEST_OK)
        return TEST_FAILED;
    if (testParseCpuSetFail("1,2 ") != TEST_OK)
        return TEST_FAILED;

    if (testParseCpusetValues1() != TEST_OK)
        return TEST_FAILED;

    return TEST_OK;
}
