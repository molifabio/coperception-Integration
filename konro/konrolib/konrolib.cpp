#include "konrolib.h"

#include "../lib/httplib/httplib.h"
#include "../lib/json/json.hpp"
#include <string>
#include <cstdlib>
#include <cerrno>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef TIMING
#include <chrono>
#include <iostream>
#endif

namespace konro {

namespace {

    /*!
     * \brief Gets the process name
     */
    std::string getProgramName() {
#if defined(_GNU_SOURCE)
        return program_invocation_name;
#else
        return "?";
#endif
    }

    /*!
     * \brief Gets Konro's HTTP server address
     */
    std::string getServerAddress() {
        const char *envAddr = getenv("KONRO");
        if (envAddr == nullptr)
            return "http://localhost:8080";
        else
            return envAddr;
    }

    /*!
     * \brief Sends an HTTP POST to Konro's server
     */
    std::string sendPost(const std::string &msgType, const std::string &text) {
        httplib::Client cli(getServerAddress());
        std::string addr = "/" + msgType;
        httplib::Result result = cli.Post(addr.c_str(), text, "application/json");
        if (result) {
            const httplib::Response &response = result.value();
            return response.body;
        } else {
            return "";
        }
    }

#define PID_FNAME "/proc/self/ns/pid"

    /*!
     * \brief Gets the Linux PID namespace to which the process belongs
     */
    unsigned long getPidNamespace() {
        unsigned long rc = 0;

        int fd = open(PID_FNAME, O_RDONLY);
        if (fd == -1) {
            perror("open " PID_FNAME);
        } else {
            struct stat sb;
            if (fstat(fd, &sb) == -1) {
                perror("fstat fd of " PID_FNAME);
            } else {
                rc = sb.st_ino;
            }
            close(fd);
        }
        return rc;
    }

#undef PID_FNAME
}

template<typename T>
T limit_between(T val, T minval, T maxval)
{
    return std::max(std::min(val, maxval), minval);
}

int computeFeedback(int curValue, int target)
{
    constexpr int minFeedback = 0;
    constexpr int maxFeedback = 200;
    int feedback = (curValue * 100) / target;
    return limit_between(feedback, minFeedback, maxFeedback);
}

std::string sendFeedbackMessage(int feedback)
{
#ifdef TIMING
    using namespace std::chrono;
    high_resolution_clock::time_point _start_ = high_resolution_clock::now();
    microseconds us;
#endif
    nlohmann::json j;
    j["pid"] = getpid();
    j["feedback"] = feedback;
    j["namespace"] = getPidNamespace();
    std::string result= sendPost("feedback", j.dump());
#ifdef TIMING
    high_resolution_clock::time_point _end_ = high_resolution_clock::now();
    microseconds elapsed = std::chrono::duration_cast<microseconds>(_end_ - _start_);
    std::cout << "KONROLIB timing: sendFeedbackMessage = "
              << elapsed.count()
              << " microseconds"
              << std::endl;
#endif
    return result;
}

std::string sendAddMessage()
{
#ifdef TIMING
    using namespace std::chrono;
    high_resolution_clock::time_point _start_ = high_resolution_clock::now();
    microseconds us;
#endif

    nlohmann::json j;
    j["pid"] = getpid();
    j["namespace"] = getPidNamespace();
    j["name"] = getProgramName();

    std::string out = sendPost("add", j.dump());

#ifdef TIMING
    high_resolution_clock::time_point _end_ = high_resolution_clock::now();
    microseconds elapsed = std::chrono::duration_cast<microseconds>(_end_ - _start_);
    std::cout << "KONROLIB timing: sendAddMessage = "
              << elapsed.count()
              << " microseconds"
              << std::endl;
#endif
    return out;
}

}   // namespace feedback
