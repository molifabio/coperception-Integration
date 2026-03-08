#include "konromanager.h"
#include "threadname.h"
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <memory>
#include <log4cpp/Appender.hh>
#include <log4cpp/FileAppender.hh>
#include <log4cpp/OstreamAppender.hh>
#include <log4cpp/Layout.hh>
#include <log4cpp/BasicLayout.hh>
#include <log4cpp/SimpleLayout.hh>
#include <log4cpp/PatternLayout.hh>
#include <log4cpp/Priority.hh>

static void trapCtrlC();

// KonroManager is declared at file scope in order
// to be usable from ctrlCHandler
static KonroManager *konroManager;

/*!
 * Configures log4cpp
 */
static void setupLogging(const std::string &level)
{
    // Log4CPP configuration

    log4cpp::Appender *appender1 = new log4cpp::OstreamAppender("console", &std::cout);
    log4cpp::PatternLayout *layout1 = new log4cpp::PatternLayout();
    layout1->setConversionPattern("%d [%p] %m%n");
    appender1->setLayout(layout1);

    log4cpp::Appender *appender2 = new log4cpp::FileAppender("logfile", "konro.log");
    log4cpp::PatternLayout *layout2 = new log4cpp::PatternLayout();
    layout2->setConversionPattern("%d [%p] %m%n");
    appender2->setLayout(layout2);

    log4cpp::Category &cat = log4cpp::Category::getRoot();
    log4cpp::Priority::PriorityLevel prio = log4cpp::Priority::INFO;
    if (!level.empty()) {
        if (level == "FATAL") {
            prio = log4cpp::Priority::FATAL;
        } else if (level == "ALERT") {
            prio = log4cpp::Priority::ALERT;
        } else if (level == "CRIT") {
            prio = log4cpp::Priority::CRIT;
        } else if (level == "ERROR") {
            prio = log4cpp::Priority::ERROR;
        } else if (level == "WARN") {
            prio = log4cpp::Priority::WARN;
        } else if (level == "NOTICE") {
            prio = log4cpp::Priority::NOTICE;
        } else if (level == "INFO") {
            prio = log4cpp::Priority::INFO;
        } else if (level == "DEBUG") {
            prio = log4cpp::Priority::DEBUG;
        } else if (level == "NOTSET") {
            prio = log4cpp::Priority::NOTSET;
        }
    }
    cat.setPriority(prio);
    cat.addAppender(appender1);
    cat.addAppender(appender2);

    cat.info("KONRO starting");
}

int main(int argc, char *argv[])
{
    std::string configFile, logLevel = "DEBUG";
    int c, optionIndex;
    static struct option longOptions[] = {
        {"config",   required_argument, 0, 'c' },
        {"loglevel", optional_argument, 0, 'l' },
        {0,          0,                 0,  0  }
    };

    while (true) {
        c = getopt_long(argc, argv, "c:l:", longOptions, &optionIndex);
        if (c == -1)
            break;      // all options parsed)
        switch (c) {
        case 'c':
            configFile = optarg;
            break;
        case 'l':
            logLevel = optarg;
            break;
        default:
            std::cout << "Unknown option\n"
                      << "Usage: konro [--config,-c  CONFIG_FILE] [--loglevel,-l LOGLEVEL]\n"
                      << "       CONFIG_FILE is the name (with path) of the KONRO configuration file\n"
                      << "       LOGLEVEL can be FATAL, ALERT, CRIT, ERROR, WARN, NOTICE, INFO, DEBUG, NOTSET\n";
            exit(EXIT_FAILURE);
        }
    }
    // ignore remaining (non option) arguments

    rmcommon::setThreadName("MAIN");
    setupLogging(logLevel);
    trapCtrlC();
    log4cpp::Category::getRoot().info("MAIN creating KonroManager");
    konroManager = new KonroManager(configFile);
    konroManager->run();

    // KonroManager must be deleted before exiting from main
    // or a segmentation fault occurs in log4cpp
    delete konroManager;

    log4cpp::Category::getRoot().info("MAIN exiting");
    return EXIT_SUCCESS;
}

static void ctrlCHandler(int s)
{
    puts("Ctrl-C");
    log4cpp::Category::getRoot().info("MAIN Ctrl-C: stopping konro");
    konroManager->stop();
}

static void trapCtrlC()
{
#if 1
    signal(SIGINT, ctrlCHandler);
#else
    struct sigaction sig_action;

    memset(&sig_action,0, sizeof(sig_action));
    sig_action.sa_handler = ctrlCHandler;
    sigemptyset(&sig_action.sa_mask);
    sig_action.sa_flags = 0;
    sig_action.sa_restorer = nullptr;

    sigaction(SIGINT, &sig_action, nullptr);
#endif
}
