#include "basethread.h"

using namespace std;

namespace rmcommon {

BaseThread::BaseThread() : stop_(false)
{
}

BaseThread::~BaseThread()
{
}

void BaseThread::start()
{
    stop_= false;
    baseThread_ = thread(&BaseThread::run, this);
}

void BaseThread::stop()
{
    stop_ = true;
}

void BaseThread::join()
{
    if (baseThread_.joinable()) {
        baseThread_.join();
    }
}

}   // namespace rmcommon
