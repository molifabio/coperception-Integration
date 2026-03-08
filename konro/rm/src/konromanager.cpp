#include "config.h"
#include "konromanager.h"
#include "dir.h"
#include "makepath.h"
#include "constants.h"
#include "proclistener.h"
#include "policymanager.h"
#include "workloadmanager.h"
#include "platformmonitor.h"
#include "proclistener.h"
#include "konrohttp.h"
#include "policytimer.h"
#include "eventbus.h"
#include <unistd.h>
#include <log4cpp/Appender.hh>
#include <log4cpp/FileAppender.hh>
#include <log4cpp/OstreamAppender.hh>
#include <log4cpp/Layout.hh>
#include <log4cpp/BasicLayout.hh>
#include <log4cpp/SimpleLayout.hh>
#include <log4cpp/PatternLayout.hh>
#include <log4cpp/Priority.hh>

/*!
 * Helper function to read from configuration file
 *
 * \return the value read from the configuration file
 */
template<typename T>
T configRead(konro::Config config, const char *section, const char *key, const T &&defaultValue) {
    try {
        return config.read<T>(section, key);
    } catch (std::logic_error &e) {
        log4cpp::Category::getRoot().info("MAIN could not read key %s from Konro configuration file", key);
        return defaultValue;
    } catch (...) {
        log4cpp::Category::getRoot().info("MAIN error reading key %s from Konro configuration file", key);
        return defaultValue;
    }
}

struct KonroManager::KonroManagerImpl {
    rmcommon::EventBus eventBus;
    pc::CGroupControl cgc;
    PlatformDescription platformDescription;
    wm::ProcListener *procListener;
    wm::WorkloadManager *workloadManager;
    http::KonroHttp *http;
    rp::PolicyManager *policyManager;
    rp::PolicyTimer *policyTimer;
    PlatformMonitor *platformMonitor;

    KonroManagerImpl() {
        procListener = nullptr;
        workloadManager = nullptr;
        http = nullptr;
        policyManager = nullptr;
        policyTimer = nullptr;
        platformMonitor = nullptr;
    }

    ~KonroManagerImpl() {
        delete procListener;
        delete workloadManager;
        delete http;
        delete policyManager;
        delete policyTimer;
        delete platformMonitor;
    }
};

KonroManager::KonroManager(std::string configFile) :
    pimpl_(new KonroManagerImpl()),
    cat_(log4cpp::Category::getRoot())
{
    loadConfiguration(configFile);
}

KonroManager::~KonroManager()
{
}

std::string KonroManager::defaultConfigFilePath()
{
    std::string home = rmcommon::Dir::home();
    if (home.empty())
        return CONFIG_PATH;
    else
        return rmcommon::make_path(home, CONFIG_PATH);
}

void KonroManager::loadConfiguration(std::string configFile)
{
    if (configFile.empty())
        configFile = defaultConfigFilePath();

    cat_.info("MAIN Konro configuration file is %s", configFile.c_str());

    if (!rmcommon::Dir::file_exists(configFile.c_str())) {
        cat_.error("MAIN Konro configuration %s not found", configFile.c_str());
        return;
    }

    const konro::Config &config = konro::Config::get(configFile);

    cfgPolicyName_ = configRead(config, "policy", "policy", std::string("NoPolicy"));
    cfgTimerSeconds_ = configRead(config, "policytimer", "timerseconds", 30);
    cfgMonitorPeriod_ = configRead(config, "platformmonitor", "monitorperiod", 20);
    cfgCpuModuleNames_ = configRead(config, "platformmonitor", "kernelcpumodulenames", std::string("coretemp,k10temp,k8temp,cputemp"));
    cfgBatteryModuleNames_ = configRead(config, "platformmonitor", "kernelbatterymodulenames", std::string("BAT"));
    httpListenHost_ = configRead(config, "http", "listenhost", std::string("localhost"));
    httpListenPort_ = configRead(config, "http", "listenport", 8080);
    changeContainerCgroup_ = configRead(config, "container", "changecontainercgroup", 1);
    changeKubernetesCgroup_ = configRead(config, "kubernetes", "changekubernetescgroup", 1);

    cat_.info("MAIN configuration: policy = %s", cfgPolicyName_.c_str());
    cat_.info("MAIN configuration: policy timer seconds = %d", cfgTimerSeconds_);
    cat_.info("MAIN configuration: monitor period seconds = %d", cfgMonitorPeriod_);
    cat_.info("MAIN configuration: CPU module names = %s", cfgCpuModuleNames_.c_str());
    cat_.info("MAIN configuration: battery module names = %s", cfgBatteryModuleNames_.c_str());
    cat_.info("MAIN configuration: HTTP listen on %s:%d", httpListenHost_.c_str(), httpListenPort_);
    cat_.info("MAIN configuration: change container cgroup = %s",
              changeContainerCgroup_ ? "true" : "false");
    cat_.info("MAIN configuration: change Kubernetes cgroup = %s",
              changeKubernetesCgroup_ ? "true" : "false");
}

void KonroManager::run()
{
    rp::PolicyManager::Policy policy = rp::PolicyManager::getPolicyByName(cfgPolicyName_);

    pimpl_->cgc.cleanup();
    pimpl_->cgc.setChangeContainerCgroup(changeContainerCgroup_);
    pimpl_->cgc.setChangeKubernetesCgroup(changeKubernetesCgroup_);
    pimpl_->http = new http::KonroHttp(pimpl_->eventBus, httpListenHost_.c_str(), httpListenPort_);
    pimpl_->policyManager = new rp::PolicyManager(pimpl_->eventBus, pimpl_->platformDescription, policy);
    pimpl_->workloadManager = new wm::WorkloadManager(pimpl_->eventBus, pimpl_->cgc);
    pimpl_->procListener = new wm::ProcListener(pimpl_->eventBus);
    pimpl_->platformMonitor = new PlatformMonitor(pimpl_->eventBus, pimpl_->platformDescription, cfgMonitorPeriod_);
    pimpl_->policyTimer = new rp::PolicyTimer(pimpl_->eventBus, cfgTimerSeconds_);

    pimpl_->platformDescription.logTopology();
    pimpl_->platformMonitor->setCpuModuleNames(cfgCpuModuleNames_);
    pimpl_->platformMonitor->setBatteryModuleNames(cfgBatteryModuleNames_);

    // Note on threads:
    //
    // 1. ProcListener runs in the current (main/KonroManager) thread
    // 2. WorkloadManager runs in a separate thread
    // 3. PolicyManager runs in a separate thread
    // 4. PlatformMonitor runs in a separate thread
    // 5. KonroHttp runs in a separate thread
    // 6. PolicyTimer runs in a separate thread

    cat_.info("MAIN starting WorkloadManager thread");
    pimpl_->workloadManager->start();

    cat_.info("MAIN starting PolicyManager thread");
    pimpl_->policyManager->start();

    /* PolicyTimer is an optional thread */
    if (cfgTimerSeconds_ > 0)  {
        cat_.info("MAIN starting PolicyTimer thread");
        pimpl_->policyTimer->start();
    } else {
        cat_.info("MAIN PolicyTimer thread not started (timerseconds is %d)", cfgTimerSeconds_);
    }

    cat_.info("MAIN starting PlatformMonitor thread");
    pimpl_->platformMonitor->start();

    cat_.info("MAIN starting HTTP thread");
    pimpl_->http->start();

    cat_.info("MAIN running ProcListener in the main thread");
    pimpl_->procListener->run();

    // when ProcListener returns we stop all the threads

    cat_.info("KONROMANAGER stopping threads");

    pimpl_->http->stop();
    if (cfgTimerSeconds_ > 0)  {
        pimpl_->policyTimer->stop();
    }
    pimpl_->platformMonitor->stop();
    pimpl_->workloadManager->stop();
    pimpl_->policyManager->stop();

    // ... and finally join all the threads

    cat_.info("KONROMANAGER joining threads");

    pimpl_->http->join();
    if (cfgTimerSeconds_ > 0)  {
        pimpl_->policyTimer->join();
    }
    pimpl_->platformMonitor->join();
    pimpl_->workloadManager->join();
    pimpl_->policyManager->join();

    cat_.info("KONROMANAGER exiting");
}

void KonroManager::stop()
{
    pimpl_->procListener->stop();
}

void KonroManager::testPlatformDescription()
{
    PlatformDescription pd;
    pd.logTopology();
}
