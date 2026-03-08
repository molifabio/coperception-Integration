#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include <iostream>
#include <ctime>
#include <cstring>

namespace rmcommon {

/*!
 * A simple class for elapsed time measurement.
 *
 * T std::chrono::nanoseconds, ...,  std::chrono::minutes
 *
 * \code
 *  Timer<std::chrono::microseconds> t;
 *
 *  t.Reset()
 *  ... code
 *  std::chrono::microseconds us = t.Elapsed();
 *  std::cout << us.count() << " microseconds elapsed\n";
 * \endcode
 */
template<typename T>
class Timer {
public:
    typedef T TimeUnit;
    typedef std::chrono::high_resolution_clock::time_point TimePoint;

    enum TimerMode {
        TIMER_CONTINUE,     // continue with the same initial time point
        TIMER_RESTART       // reset initial time point ot "now"
    };

    explicit Timer() {
        Restart();
    }

    /*!
     * Stores the current time point as starting point for
     * elapsed time measurement
     */
    void Restart() {
        start_ = std::chrono::high_resolution_clock::now();
    }

    static TimePoint now() {
        return std::chrono::high_resolution_clock::now();
    }

    /*!
     * Returns the duration between the current time point and the
     * "start" time point
     *
     * \param start the starting poin of the duration
     */
    static T ElapsedFrom(TimePoint start) {
        T val = std::chrono::duration_cast<T>(now() - start);
        return val;
    }

    /*!
     * Returns the duration between the current time point and the
     * time point stored by the last Reset() call
     *
     * \param mode if TIMER_RESET, the current time point becomes the new
     *             starting point for duration calculation (i.e. a new
     *             measurement starts
     */
    T Elapsed(TimerMode mode = TIMER_CONTINUE) {
        TimePoint cur = std::chrono::high_resolution_clock::now();
        T val = ElapsedFrom(start_);
        if (mode == TIMER_RESTART)
            Restart();
        return val;
    }

    template <typename U, typename Traits>
    friend std::basic_ostream<U, Traits>& operator<<(std::basic_ostream<U, Traits>& out,
                                                     const Timer& timer) {
        return out << timer.Elapsed().count();
    }

    static timespec timepointToTimespec(std::chrono::high_resolution_clock::time_point tp) {
        auto secs = std::chrono::time_point_cast<std::chrono::seconds>(tp);
        if (secs > tp)
            secs = secs - std::chrono::seconds{1};
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(tp - secs);
        return timespec{secs.time_since_epoch().count(), ns.count()};
    }

    /*!
     * Converts a C struct timespec to a chrono time_point
     *
     * \param ts the timespec to convert
     * \return the time_point
     */
    static std::chrono::high_resolution_clock::time_point timespecToTimepoint(struct timespec ts) {
        return std::chrono::high_resolution_clock::time_point{
            std::chrono::seconds{ts.tv_sec} + std::chrono::nanoseconds{ts.tv_nsec}};
    }

    static long getSystemMicroseconds() {
        struct timespec ts;
        memset(&ts, 0, sizeof(ts));
        clock_gettime(CLOCK_REALTIME, &ts);
        return ts.tv_sec * 1000000L + ts.tv_nsec / 1000L;
    }
private:
    TimePoint start_;
};

typedef Timer<std::chrono::microseconds>    KonroTimer;

}   //  namespace rmcommon

#endif // TIMER_H
