#ifndef TIME_TRACKING_REPORT_THREADSAFEQUEUE_H
#define TIME_TRACKING_REPORT_THREADSAFEQUEUE_H

#include <condition_variable>
#include <mutex>
#include <queue>

// We can use Boost.Lockfree alternatively
// TODO: Add size restriction
template<typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() = default;

    void push(T val)
    {
        {
            std::lock_guard<std::mutex> lck(_m);
            _queue.push(val);
        }
        _cv.notify_one();
    }

    T wait_and_pop()
    {
        std::unique_lock<std::mutex> lck(_m);
        _cv.wait(lck, [this](){ return !_queue.empty(); });
        T val = _queue.front();
        _queue.pop();
        return val;
    }

private:
    std::queue<T> _queue;

    std::mutex _m;
    std::condition_variable _cv;
};


#endif //TIME_TRACKING_REPORT_THREADSAFEQUEUE_H
