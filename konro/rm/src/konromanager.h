#ifndef KONROMANAGER_H
#define KONROMANAGER_H

#include <string>
#include <memory>
#include <log4cpp/Category.hh>

class KonroManager {
    struct KonroManagerImpl;
    std::unique_ptr<KonroManagerImpl> pimpl_;
    log4cpp::Category &cat_;

    // values from configuration file
    std::string cfgPolicyName_;
    int cfgTimerSeconds_;       // 0 means "no timer"
    int cfgMonitorPeriod_;
    std::string cfgCpuModuleNames_;
    std::string cfgBatteryModuleNames_;
    std::string httpListenHost_;
    int httpListenPort_;
    bool changeContainerCgroup_;
    bool changeKubernetesCgroup_;

    std::string defaultConfigFilePath();
    void setupLogging();
    void loadConfiguration(std::string configFile);
public:
    KonroManager(std::string configFile = "");
    ~KonroManager();

    KonroManager(const KonroManager &) = delete;
    KonroManager &operator=(const KonroManager &) = delete;
    KonroManager(KonroManager &&) noexcept = delete;
    KonroManager &operator=(KonroManager &&) noexcept = delete;

    /*!
     * Configures and starts the ProcListener in the main thread
     * and all the other Konro application threads
     */
    void run();

    /*!
     * Stops the ProcListener and all the threads
     */
    void stop();

    void testPlatformDescription();
};

#endif // KONROMANAGER_H
