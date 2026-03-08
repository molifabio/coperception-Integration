#include <iostream>
#include <map>
#include <algorithm>
#include <cstdlib>
#include "cgroupcontrol.h"
#include "cpucontrol.h"
#include "iocontrol.h"
#include "cpusetcontrol.h"
#include "cpusetvector.h"
#include "memorycontrol.h"
#include "cgrouputil.h"
#include "numericvalue.h"
#include "pcexception.h"

using namespace std;
using namespace pc;

static void checkAppDir(int pid)
{
    string realDir = pc::util::findCgroupPath(static_cast<pid_t>(pid));
    string wantedDir = pc::util::getCgroupKonroAppDir(static_cast<pid_t>(pid));
    if (realDir != wantedDir) {
        cerr << "checkAppDir: invalid app base dir\n";
        exit(EXIT_FAILURE);
    }
    cout << pid << " base directory is " << realDir << endl;
}

static void setCpuMax(std::shared_ptr<rmcommon::App> app, int n)
{
    //pc::CpuControl().setValue(pc::CpuControl::MAX, n, app);
    pc::CpuControl::instance().setMax(n, app);
    rmcommon::NumericValue nv = pc::CpuControl::instance().getMax(app);
    cout << "Cpu max: requested " << n << " read " << nv << endl;
    if (nv != n)
        cout << "ERROR setCpuMax\n";
    else
        cout << "setCpuMax OK\n";
}

static void setCpuMax(std::shared_ptr<rmcommon::App> app)
{
    pc::CpuControl::instance().setMax(rmcommon::NumericValue::max(), app);
    rmcommon::NumericValue nv = pc::CpuControl::instance().getMax(app);
    cout << "Cpu max: requested " << rmcommon::NumericValue::max() << " read " << nv << endl;
    if (!nv.isMax())
        cout << "ERROR setCpuMax(max)\n";
    else
        cout << "setCpuMax(max) OK\n";
}

static void getCpuStat(std::shared_ptr<rmcommon::App> app)
{
    map<string, uint64_t> tags = pc::CpuControl::instance().getStat(app);
    cout << "CPU STAT\n";
    for (const auto& kv : tags) {
        cout << "    " << kv.first << ":" << kv.second << endl;
    }
}

static void getIoMax(std::shared_ptr<rmcommon::App> app, int major, int minor)
{
    cout << "Setting max wbps\n";
    pc::IOControl::instance().setMax(8, 0, pc::IOControl::WBPS, 1000000, app);
    sleep(2);
    map<string, rmcommon::NumericValue> tags = pc::IOControl::instance().getMax(major, minor, app);
    cout << "IO MAX\n";
    for (const auto& kv : tags) {
        cout << "    " << kv.first << ":" << kv.second << endl;
    }
    if (tags["wbps"] != 1000000)
        cout << "ERROR getIoMax\n";
    else
        cout << "getIoMax OK\n";
}

static void getIoStat(std::shared_ptr<rmcommon::App> app, int major, int minor)
{
    map<string, rmcommon::NumericValue> tags = pc::IOControl::instance().getStat(major, minor, app);
    cout << "IO STAT\n";
    for (const auto& kv : tags) {
        cout << kv.first << ":" << kv.second << endl;
    }
}

static void setCpuSet(std::shared_ptr<rmcommon::App> app)
{
    pc::CpusetControl::instance().setCpus({ {0, 0}, {3, 3} }, app);
    rmcommon::CpusetVector cpus = pc::CpusetControl::instance().getCpus(app);
    if (std::find(begin(cpus), end(cpus), std::make_pair((short)0, (short)0)) == cpus.end() ||
            std::find(begin(cpus), end(cpus), std::make_pair((short)3, (short)3)) == cpus.end()) {
        cout << "ERROR setCpuSet\n";
    } else {
        cout << "setCpuSet OK\n";
    }
}

static void testMemoryControl(std::shared_ptr<rmcommon::App> app)
{
    pc::MemoryControl::instance().setMin(16384, app);
    int mmin = pc::MemoryControl::instance().getMin(app);
    if (mmin != 16384) {
        cout << "ERROR testMemoryControl setMemoryMin. Requested 16384, got " << mmin << "\n";
        return;
    }

    pc::MemoryControl::instance().setMax(98304, app);
    int mmax = pc::MemoryControl::instance().getMax(app);
    if (pc::MemoryControl::instance().getMax(app) != 98304) {
        cout << "ERROR testMemoryControl setMemoryMax. Requested 100000, got " << mmax << "\n";
        return;
    }

    map<string, uint64_t> events = pc::MemoryControl::instance().getEvents(app);
    cout << "MEMORY EVENTS\n";
    for (const auto &ev: events) {
        cout << "    " << ev.first << ' ' << ev.second << endl;
    }
    cout << "testMemoryControl OK\n";
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        cerr << "Missing parameter: PID\n";
        exit(EXIT_FAILURE);
    }
    int pid = atoi(argv[1]);

    pc::CGroupControl cgc;
    std::shared_ptr<rmcommon::App> app = rmcommon::App::makeApp(pid, rmcommon::App::AppType::STANDALONE);

    int major = 8, minor = 0;
    try {
        cgc.addApplication(app);
        checkAppDir(pid);
        testMemoryControl(app);
        setCpuMax(app, 33);
        setCpuMax(app);
        getCpuStat(app);
        setCpuSet(app);
        getIoStat(app, major, minor);
        getIoMax(app, major, minor);
    } catch (pc::PcException &e) {
        cerr << "PcException: " << e.what() << endl;
        exit(EXIT_FAILURE);
    }

}
