#include "konrohttp.h"
#include "threadname.h"
#include "namespaces.h"
#include "../../lib/httplib/httplib.h"
#include "../../lib/json/json.hpp"
#include "feedbackrequestevent.h"
#include "addrequestevent.h"
#include "app.h"

#ifdef TIMING
#include "timer.h"
#endif

using namespace std;

namespace http {

struct KonroHttp::KonroHttpImpl {
    rmcommon::EventBus &bus_;
    log4cpp::Category &cat_;
    httplib::Server srv;

    KonroHttpImpl(rmcommon::EventBus &eventBus) :
        bus_(eventBus),
        cat_(log4cpp::Category::getRoot()) {
    }

    /*!
     * Extracts the data from the JSON and publishes a FeedbackRequestEvent
     *
     * \param data the JSON in text format
     */
    void sendFeedbackEvent(const std::string &data) {
        using namespace nlohmann;
        rmcommon::KonroTimer::TimePoint tp = rmcommon::KonroTimer::now();
        basic_json<> j = json::parse(data);
        rmcommon::namespace_t ns = 0;
        /* PID and feedback value must always be present */
        if (!j.contains("pid") || !j.contains("feedback")) {
            cat_.error("KONROHTTP missing pid or feedback value in feedback message");
            return;
        }
        pid_t pid = j["pid"];
        int feedback = j["feedback"];
        /* If no namespace is present, the process belongs to Konro's ns */
        if (j.contains("namespace")) {
            ns = j["namespace"];
        }
        cat_.info("KONROHTTP publishing FeedbackRequestEvent with value %d from pid %ld in namespace %lu",
                  feedback,
                  static_cast<long>(pid),
                  ns);
        rmcommon::FeedbackRequestEvent *event = new rmcommon::FeedbackRequestEvent(pid, ns, feedback);
        event->setTimePoint(tp);
        bus_.publish(event);
   }

    /*!
     * Extracts the data from the JSON and publishes an AddRequestEvent
     *
     * \param data the JSON in text format
     */
    void sendAddEvent(const std::string &data) {
        using namespace nlohmann;
        rmcommon::KonroTimer::TimePoint tp = rmcommon::KonroTimer::now();
        basic_json<> j = json::parse(data);
        /* This is the PID of the process in its own namespace */
        pid_t nsPid;
        /* This is the PID of the process in Konro's namespace */
        pid_t pid;
        rmcommon::App::AppType appType = rmcommon::App::AppType::INTEGRATED;
        string name = "";
        rmcommon::namespace_t ns = 0;
        /* PID must always be present */
        if (!j.contains("pid")) {
            cat_.error("KONROHTTP missing pid in add message");
            return;
        }
        nsPid = j["pid"];
        /* Application name is optional */
        if (j.contains("name")) {
            name = j["name"];
        }
        /* Type is only sent for standalone processes */
        /* Integrated apps type must instead be inferred using their namespace */
        /* If neither between type and namespace is present, message is invalid */
        if (j.contains("type")) {
            string type = j["type"];
            appType = rmcommon::App::getTypeByName(type);
            if (appType == rmcommon::App::AppType::UNKNOWN) {
                cat_.error("KONROHTTP invalid application type %s", type.c_str());
                return;
            }
            // The received pid is the one inside Konro's namespace
            pid = nsPid;
        } else if (j.contains("namespace")) {
            ns = j["namespace"];
            // Map the PID we received to Konro's namespace
            pid = rmcommon::mapPid(nsPid, ns);
            // If true, the process is in a different PID ns from Konro
            if (pid != nsPid) {
                appType = (rmcommon::isKubPod(pid))
                        ? rmcommon::App::AppType::KUBERNETES
                        : rmcommon::App::AppType::CONTAINER;
            }
        } else {
            cat_.error("KONROHTTP invalid message: no type or ns specified");
            return;
        }
        cat_.info("KONROHTTP publishing AddRequestEvent for pid %ld in ns %lu with name \"%s\" and type \"%s\"",
                  static_cast<long>(nsPid),
                  ns,
                  name.c_str(),
                  rmcommon::App::getAppTypeString(appType).c_str());
        rmcommon::AddRequestEvent *event =
                new rmcommon::AddRequestEvent(rmcommon::App::makeApp(pid, appType, name, nsPid, ns));
        event->setTimePoint(tp);
        bus_.publish(event);
    }

    void handleGet(const httplib::Request &req, httplib::Response &res) {
        cat_.info("HTTP GET received");
        res.status = 200;
        res.set_content("200 - OK\r\n", "text/html");
    }

    /*!
     * \brief handles the addition of a new process to Konro
     */
    void handleAddPost(const httplib::Request &req, httplib::Response &res, const httplib::ContentReader &content_reader){
        cat_.info("KONROHTTP ADD POST received");
#ifdef TIMING
        cat_.debug("KONROHTTP timing: ADD POST received at %ld system microseconds",
                   rmcommon::KonroTimer::getSystemMicroseconds());
#endif
        std::string body;
        content_reader([&](const char *data, size_t data_length) {
                body.append(data, data_length);
                return true;
            });
        res.set_content("You have sent an ADD POST '" + body + "'\r\n", "text/plain");
        sendAddEvent(body);
    }

    /*!
     * \brief handles a feedback message sent by an integrated application
     */
    void handleFeedbackPost(const httplib::Request &req, httplib::Response &res, const httplib::ContentReader &content_reader){
        cat_.info("KONROHTTP FEEDBACK POST received");
        std::string body;
        content_reader([&](const char *data, size_t data_length) {
                body.append(data, data_length);
                return true;
            });
        res.set_content("You have sent a FEEDBACK POST '" + body + "'\r\n", "text/plain");
        sendFeedbackEvent(body);
    }
};

KonroHttp::KonroHttp(rmcommon::EventBus &eventBus, const char *listen_host, int listen_port) :
    pimpl_(new KonroHttpImpl(eventBus)),
    cat_(log4cpp::Category::getRoot()),
    listen_host_(listen_host),
    listen_port_(listen_port)
{
}

KonroHttp::~KonroHttp()
{
}

void KonroHttp::stop()
{
    pimpl_->srv.stop();
    BaseThread::stop();     // not really needed here because there is no loop
    cat_.info("KONROHTTP server stopped");
}

void KonroHttp::run()
{
    setThreadName("KONROHTTP");
    cat_.info("KONROHTTP thread starting");

    /* Test if server is running */
    pimpl_->srv.Get("/status", [this](const httplib::Request &req, httplib::Response &res) {
        this->pimpl_->handleGet(req, res);
    });

    /* Add new process under Konro's management */
    pimpl_->srv.Post("/add", [this](const httplib::Request &req, httplib::Response &res, const httplib::ContentReader &content_reader) {
        this->pimpl_->handleAddPost(req, res, content_reader);
    });

    /* Communication with integrated applications */
    pimpl_->srv.Post("/feedback", [this](const httplib::Request &req, httplib::Response &res, const httplib::ContentReader &content_reader) {
        this->pimpl_->handleFeedbackPost(req, res, content_reader);
    });

    cat_.info("KONROHTTP server starting");

    pimpl_->srv.listen(listen_host_.c_str(), listen_port_);

    cat_.info("KONROHTTP thread exiting");
}

}   // namespace http
