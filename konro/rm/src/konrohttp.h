#ifndef KONROHTTP_H
#define KONROHTTP_H

#include "eventbus.h"
#include "basethread.h"
#include <thread>
#include <memory>
#include <log4cpp/Category.hh>

namespace http {

/*!
 * \brief a simple server running in a dedicated thread.
 * Can be used by external processes to send requests or information
 * to Konro.
 */
class KonroHttp : public rmcommon::BaseThread {
    struct KonroHttpImpl;
    std::unique_ptr<KonroHttpImpl> pimpl_;
    log4cpp::Category &cat_;
    std::string listen_host_;
    int listen_port_;

    /*! The thread function */
    virtual void run() override;

public:
    explicit KonroHttp(rmcommon::EventBus &eventBus, const char *listen_host = "localhost", int listen_port = 8080);
    ~KonroHttp();

    virtual void stop() override;
};

}   // namespace http

#endif // KONROHTTP_H
