#ifndef THREADSAFEQUEUE_H
#define THREADSAFEQUEUE_H

#include <mutex>
#include <queue>
#include <chrono>
#include <condition_variable>

namespace rmcommon {

/*!
 * \brief A simple threadsafe queue class
 */
template<typename T>
class ThreadsafeQueue {
    mutable std::mutex mut_;
    std::condition_variable cond_;
    std::queue<T> data_;
public:
    ThreadsafeQueue() = default;

    void push(T new_value) {
        std::lock_guard<std::mutex> lck(mut_);
        data_.push(std::move(new_value));
        cond_.notify_one();
    }

    /*!
     * Waits until an event is received.
     * When an event is received, it is copied in "value"
     * and the function returns.
     *
     * \param value the received event
     */
    void waitAndPop(T &value) {
        std::unique_lock<std::mutex> lck(mut_);
        cond_.wait(lck, [this] {return !data_.empty(); });
        value = std::move(data_.front());
        data_.pop();
    }

    /*!
     * Waits for the specified number of milliseconds for an event.
     * If an event is received, it is copied in "value" and true is
     * returned.  Otherwise, false is returned.
     *
     * \param value the received event
     * \param millis number of milliseconds to wait for the event
     * \return true if an evetn is received
     */
    bool waitAndPop(T &value, std::chrono::milliseconds millis) {
        std::unique_lock<std::mutex> lck(mut_);
        if (!cond_.wait_for(lck, millis,
                           [this] { return !data_.empty(); })) {
            return false;
        }
        value = std::move(data_.front());
        data_.pop();
        return true;
    }

    bool tryPop(T &value) {
        std::lock_guard<std::mutex> lck(mut_);
        if (data_.empty())
            return false;
        value = std::move(data_.front());
        data_.pop();
        return true;
    }

    bool empty() {
        std::lock_guard<std::mutex> lck(mut_);
        return data_.empty();
    }
};

}   // namespace rmcommon

#endif // THREADSAFEQUEUE_H
