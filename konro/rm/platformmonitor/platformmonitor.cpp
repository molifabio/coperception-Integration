#include "platformmonitor.h"
#include <chrono>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <sensors.h>
#include "componenttemperature.h"
#include "monitorevent.h"
#include "platformtemperature.h"
#include "platformpower.h"
#include "platformload.h"
#include "threadname.h"
#include "tsplit.h"
#include "cputimedata.h"

using namespace std;

struct PlatformMonitor::PlatformMonitorImpl {
    PlatformDescription pd_;
    bool initialized = false;
    vector<string> cpuChips;
    vector<string> batteryChips;
    vector<CPUTimeData> cpuTimeData;

    PlatformMonitorImpl(PlatformDescription pd) : pd_(pd) {
        init();
    }

    ~PlatformMonitorImpl() {
        fini();
    }

    void init() {
        this->initialized = sensors_init(NULL) == 0;
    }

    void fini() {
        if (this->initialized)
            sensors_cleanup();
    }

    void setCpuModuleNames(const std::string &names) {
        cpuChips = rmcommon::tsplit(names, ",");
    }

    void setBatteryModuleNames(const std::string &names) {
        batteryChips = rmcommon::tsplit(names, ",");
        log4cpp::Category::getRoot().info("setBatteryModuleNames: %s", names.c_str());
        if (batteryChips.empty()) {
            log4cpp::Category::getRoot().info("setBatteryModuleNames: no battery names");
        } else {
            log4cpp::Category::getRoot().info("setBatteryModuleNames: %s", batteryChips[0].c_str());
        }
    }

    bool isCpuChip(string chip) {
        return find(begin(cpuChips), end(cpuChips), chip) != end(cpuChips);
    }

    bool isBatteryChip(string chip) {
        if (chip.empty()) {
            return false;
        }
        // Example:
        // chip is "BAT0"
        // batteryChips is [ "BAT" ]

        // remove trailing digits from string "chip"
        string::size_type pos = chip.find_first_of("0123456789");
        if (pos != string::npos) {
            chip.erase(pos);
        }
        return find(begin(batteryChips), end(batteryChips), chip) != end(batteryChips);
    }

    void handleBattery(sensors_chip_name const *cn, rmcommon::PlatformPower &platPower) {
        sensors_feature const *feat;
        int f = 0;
        while ((feat = sensors_get_features(cn, &f)) != 0) {
            if (feat->type == SENSORS_FEATURE_IN) {
                sensors_subfeature const *subf =
                        sensors_get_subfeature(cn, feat, SENSORS_SUBFEATURE_IN_INPUT);
                double val;
                int rc = sensors_get_value(cn, subf->number, &val);
                if (rc == 0)
                    platPower.setBatteryVoltage(static_cast<int>(val));
            } else if (feat->type == SENSORS_FEATURE_CURR) {
                sensors_subfeature const *subf =
                        sensors_get_subfeature(cn, feat, SENSORS_SUBFEATURE_CURR_INPUT);
                double val;
                int rc = sensors_get_value(cn, subf->number, &val);
                if (rc == 0)
                    platPower.setBatteryCurrent(static_cast<int>(val * 1000));
            }
        }
    }

    rmcommon::ComponentTemperature getTemperatureInfo(sensors_chip_name const *cn, sensors_feature const *feat) {
        rmcommon::ComponentTemperature componentTemp;
        componentTemp.label_ = sensors_get_label(cn, feat);
        componentTemp.num_ = feat->number;

        // Scan all subfeatures. Subfeatures are temperatures.
        sensors_subfeature const *subf;
        int s = 0;
        while ((subf = sensors_get_all_subfeatures(cn, feat, &s)) != 0) {
            int rc;
            double val;
            rc = sensors_get_value(cn, subf->number, &val);
            if (rc == 0) {
                switch(subf->type) {
                case SENSORS_SUBFEATURE_TEMP_INPUT:
                    componentTemp.temp_ = static_cast<int>(val);
                    break;
                case SENSORS_SUBFEATURE_TEMP_MAX:
                    componentTemp.maxTemp_ = static_cast<int>(val);
                    break;
                case SENSORS_SUBFEATURE_TEMP_MIN:
                    componentTemp.minTemp_ = static_cast<int>(val);
                    break;
                case SENSORS_SUBFEATURE_TEMP_HIGHEST:
                    componentTemp.highestTemp_ = static_cast<int>(val);
                    break;
                case SENSORS_SUBFEATURE_TEMP_LOWEST:
                    componentTemp.lowestTemp_ = static_cast<int>(val);
                    break;
                case SENSORS_SUBFEATURE_TEMP_CRIT:
                    componentTemp.critTemp_ = static_cast<int>(val);
                    break;
                case SENSORS_SUBFEATURE_TEMP_EMERGENCY:
                    componentTemp.emergencyTemp_ = static_cast<int>(val);
                    break;
                default:
                    break;
                }
            }
        }
        return componentTemp;
    }

    /*!
     * Extracts information from the specified CPU temperature sensor.
     */
    void handleCpuTemp(sensors_chip_name const *cn, rmcommon::PlatformTemperature &platTemp) {
        // Chip: coretemp - /sys/class/hwmon/hwmon3
        // 1: feature name is temp1
        // 1: feature label is Package id 0
        // 1:1: subfeature name is temp1_input/0 = 69
        // 1:2: subfeature name is temp1_max/1 = 105
        // 1:3: subfeature name is temp1_crit/2 = 105
        // 1:4: subfeature name is temp1_crit_alarm/3 = 0
        // 2: feature name is temp2
        // 2: feature label is Core 0
        // 2:5: subfeature name is temp2_input/4 = 65
        // 2:6: subfeature name is temp2_max/5 = 105
        // 2:7: subfeature name is temp2_crit/6 = 105
        // 2:8: subfeature name is temp2_crit_alarm/7 = 0
        // 3: feature name is temp3
        // 3: feature label is Core 1
        // 3:9: subfeature name is temp3_input/8 = 69
        // 3:10: subfeature name is temp3_max/9 = 105
        // 3:11: subfeature name is temp3_crit/10 = 105
        // 3:12: subfeature name is temp3_crit_alarm/11 = 0

        // Scan all features. Known features are: package(s) and core(s)
        sensors_feature const *feat;
        int f = 0;
        while ((feat = sensors_get_features(cn, &f)) != 0) {
            if (feat->type == SENSORS_FEATURE_TEMP) {
                rmcommon::ComponentTemperature componentTemp = getTemperatureInfo(cn, feat);
                char *label = componentTemp.label_;
                if (strstr(label, "Package") != nullptr) {
                    platTemp.addCpuTemperature(componentTemp);
                }
                else if (strstr(label, "Core") != nullptr) {
                    platTemp.addCoreTemperature(componentTemp);
               }
            }
        }
    }

    /*!
     * Detects each sensor available on the machine and calls the
     * appropriate handler function to store information about its
     * current status.
     */
    void handleSensors(rmcommon::PlatformTemperature &platTemp, rmcommon::PlatformPower &platPower) {
        sensors_chip_name const *cn;
        int c = 0;
        while ((cn = sensors_get_detected_chips(0, &c)) != 0) {
            if (isCpuChip(cn->prefix)) {
                // CPU temperature sensor found
                handleCpuTemp(cn, platTemp);
            }
            else if (isBatteryChip(cn->prefix)) {
                // Battery sensor found
                handleBattery(cn, platPower);
            }
        }
    }

    void handleCpuTimes(rmcommon::PlatformLoad &platLoad) {
        ifstream ifs("/proc/stat");
        bool firstRead = cpuTimeData.empty();
        int numPu = pd_.getNumProcessingUnits();
        log4cpp::Category::getRoot().debug("PLATFORMMONITOR reading CPU time data for %d processing units", numPu);
        for (int n = 0; n <= numPu; ++n) {
            string line;
            if (!getline(ifs, line)) {
                log4cpp::Category::getRoot().error("PLATFORMMONITOR Could not read line");
                break;
            }
            if (line.size() < 4 || line[0] != 'c' || line[1] !='p' || line[2] != 'u') {
                log4cpp::Category::getRoot().error("PLATFORMMONITOR Unexpected line '%s' (expected \"cpu\")", line.c_str());
                break;
            }

            {
                istringstream is(line);
                string name;
                uint64_t usertime, nicetime, systemtime, idletime, ioWait, irq, softIrq, steal, guest, guestnice;
                is >> name >> usertime >> nicetime >> systemtime >> idletime >> ioWait >> irq >> softIrq >> steal >> guest >> guestnice;
                if (is.fail()) {
                    log4cpp::Category::getRoot().error("PLATFORMMONITOR Could not parse line");
                    break;
                }

                CPUTimeData ctd(name, usertime, nicetime, systemtime, idletime, ioWait, irq, softIrq, steal, guest, guestnice);
                if (firstRead) {
                    // first time we read
                    // we only have one snapshot of the cpu status, so we can't compute usage yet
                    cpuTimeData.push_back(ctd);
                } else {
                    // time elapsed between the last two snapshots, measured in USER_HZ
                    int delta = ctd.totaltime - cpuTimeData[n].totaltime;
                    // time spent in idle between the last two snapshots, measured in USER_HZ
                    int idletime = ctd.idletime - cpuTimeData[n].idletime;
                    // percentage of use of the current processing unit
                    int usage = ((delta - idletime) * 100) / delta;
                    //log4cpp::Category::getRoot().debug("PLATFORMMONITOR result for %s is %d", ctd.name.c_str(), usage);
                    if (n == 0)
                        platLoad.addCpuLoad(usage);
                    else
                        platLoad.addPULoad(usage);
                    cpuTimeData[n] = ctd;
                }
            }
        }

        ostringstream os;
        os << "{\"cpu\":[";
        for (int n = 0; n <= numPu; ++n) {
            if (n > 0) {
                os << ',';
            }
            os << cpuTimeData[n];
        }
        os << "]}";
        //log4cpp::Category::getRoot().debug("PLATFORMMONITOR read CPU times %s", os.str().c_str());
    }
};

PlatformMonitor::PlatformMonitor(rmcommon::EventBus &eventBus, PlatformDescription pd, int monitorPeriod) :
    pimpl_(new PlatformMonitorImpl(pd)),
    cat_(log4cpp::Category::getRoot()),
    monitorPeriod_(monitorPeriod),
    bus_(eventBus)
{
}

PlatformMonitor::~PlatformMonitor()
{
}

void PlatformMonitor::setCpuModuleNames(const std::string &names)
{
    pimpl_->setCpuModuleNames(names);
}

void PlatformMonitor::setBatteryModuleNames(const std::string &names)
{
    pimpl_->setBatteryModuleNames(names);
}

void PlatformMonitor::run()
{
    setThreadName("PLATFORMMONITOR");
    cat_.info("PLATFORMMONITOR running");
    while (!stopped()) {
        for (int i = 0; i < monitorPeriod_ && !stopped() ; ++i) {
            this_thread::sleep_for(chrono::seconds(1));
        }
        if (!stopped()) {
            rmcommon::PlatformTemperature platTemp;
            rmcommon::PlatformPower platPower;
            rmcommon::PlatformLoad platLoad;
            pimpl_->handleSensors(platTemp, platPower);
            pimpl_->handleCpuTimes(platLoad);
            bus_.publish(new rmcommon::MonitorEvent(platTemp, platPower, platLoad));
        }
    }
    cat_.info("PLATFORMMONITOR exiting");
}
