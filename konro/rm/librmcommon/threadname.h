#ifndef THREADNAME_H
#define THREADNAME_H

#include <thread>
#include <pthread.h>
#include <string>

namespace rmcommon {

inline void setThreadName(const char *name) noexcept
{
    pthread_setname_np(pthread_self(), name);
}

inline std::string getThreadName()
{
    char name[32];
    pthread_getname_np(pthread_self(), name, sizeof(name));
    return name;
}

}   // namespace rmcommon

#endif // THREADNAME_H
