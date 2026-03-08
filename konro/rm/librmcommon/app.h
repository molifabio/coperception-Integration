#ifndef APP_H
#define APP_H

#include "namespaces.h"
#include <memory>
#include <string>
#include <unistd.h>

namespace rmcommon {
/*!
 * \class a class containing basic information about a generic application.
 */
class App {
public:
    enum class AppType {
        UNKNOWN,
        STANDALONE,
        INTEGRATED,
        CONTAINER,
        KUBERNETES
    };

private:
    /*! The pid of the application in Konro's namespace */
    pid_t pid_;
    /*! The type of the application */
    AppType appType_;
    /*! The name of the application */
    std::string name_;
    /*! The PID of the application in its own PID namespace.
        If 0, the app belongs to Konro's ns and this field should be ignored. */
    pid_t nsPid_;
    /*! The Linux PID namespace of the application.
        If 0, the app belongs to Konro's namespace. */
    namespace_t ns_;

    std::string cgroupDir_;

    App(pid_t pid, AppType appType, std::string appName, pid_t nsPid, namespace_t ns) :
        pid_(pid), appType_(appType), name_(appName), nsPid_(nsPid), ns_(ns) {}

public:
    typedef std::shared_ptr<App> AppPtr;

    // Disable copy, assign and move
    App(const App &rhs) = delete;
    App &operator = (const App &rhs) = delete;
    App(App &&rhs) noexcept = delete;
    App& operator=(App&& other) noexcept = delete;
    ~App() = default;

    /*!
     * \brief Factory function to create a shared_ptr to an App
     * \param pid the pid of the App
     * \param appType the type of the app
     * \returns the shared_ptr to the App
     */
    static std::shared_ptr<App> makeApp(pid_t pid, AppType appType, std::string appName = "",
                                        pid_t nsPid = 0, namespace_t ns = 0) {
        // Note: to use std::make_shared, the constructor must be public;
        //       in this context it is better to use new App(...)
        return std::shared_ptr<App>(new App(pid, appType, appName, nsPid, ns));
    }

    /*!
     * Return the app type with the specified name.
     * If no app type exists with that name, UNKNOWN is returned.
     */
    static AppType getTypeByName(const std::string &appType) noexcept {
        if (appType == "STANDALONE")
            return AppType::STANDALONE;
        else if (appType == "INTEGRATED")
            return AppType::INTEGRATED;
        else if (appType == "CONTAINER")
            return AppType::CONTAINER;
        else if (appType == "KUBERNETES")
            return AppType::KUBERNETES;
        else
            return AppType::UNKNOWN;
    }

    /*!
     * \brief Gets the pid of the application
     * \returns the pid of the application
     */
    pid_t getPid() const noexcept {
        return pid_;
    }

    /*!
     * \brief Gets the type of the application
     * \returns the type of the application
     */
    AppType getAppType() const noexcept {
        return appType_;
    }

    /*!
     * \brief Gets the PID of the application in its own namespace
     * \returns the PID of the application in its own namespace
     */
    pid_t getNsPid() const noexcept {
        return nsPid_;
    }

    /*!
     * \brief Gets the PID namespace to which the application belongs
     * \returns the PID namespace
     */
    namespace_t getPidNamespace() const noexcept {
        return ns_;
    }

    /*!
     * \brief Converts the specified app type into a string
     * \returns the specified type as string
     */
    static std::string getAppTypeString(AppType type) {
        switch (type)
        {
        case AppType::UNKNOWN: return "UNKNOWN";
        case AppType::STANDALONE: return "STANDALONE";
        case AppType::INTEGRATED: return "INTEGRATED";
        case AppType::CONTAINER: return "CONTAINER";
        case AppType::KUBERNETES: return "KUBERNETES";
        default: return "";
        }
    }

    /*!
     * \brief Gets the name of the application
     * \return the application's name
     */
    const std::string &getName() const noexcept {
        return name_;
    }

    /*!
     * \brief Sets the name of the application
     * \param name the application's name
     */
    void setName(const std::string &appName) {
        name_ = appName;
    }

    const std::string getCgroupDir() const noexcept {
        return cgroupDir_;
    }

    void setCgroupDir(const std::string &dir) {
        cgroupDir_ = dir;
    }
};

}
#endif // APP_H
