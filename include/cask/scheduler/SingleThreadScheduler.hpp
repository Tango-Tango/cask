//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_SINGLE_THREAD_SCHEDULER_H_
#define _CASK_SINGLE_THREAD_SCHEDULER_H_

#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>
#include <queue>
#include <vector>

#include "../Scheduler.hpp"

namespace cask::scheduler {
    
class SingleThreadScheduler final : public Scheduler, public std::enable_shared_from_this<SingleThreadScheduler> {
public:
    /**
     * Construct a scheduler optionally configuring the maximum number of threads
     * to use.
     * 
     * @param poolSize The number of threads to use - defaults to matching
     *                 the number of hardware threads available in the system.
     */
    SingleThreadScheduler();

    /**
     * Destruct the scheduler. Destruction waits for all running and timer
     * threads to stop before finishing.
     */
    ~SingleThreadScheduler();

    void submit(const std::function<void()>& task) override;
    void submitBulk(const std::vector<std::function<void()>>& tasks) override;
    CancelableRef submitAfter(int64_t milliseconds, const std::function<void()>& task) override;
    bool isIdle() const override;
private:
    using TimerEntry = std::tuple<int64_t, std::function<void()>>;

    bool running;
    bool idle;

    std::condition_variable dataInQueue;

    mutable std::atomic_size_t readyQueueSize;
    mutable std::atomic_flag readyQueueLock;
    std::queue<std::function<void()>> readyQueue;
    std::mutex timerMutex;
    std::map<int64_t,std::vector<TimerEntry>> timers;
    std::thread runThread;
    std::thread timerThread;
    int64_t ticks;
    int64_t next_id;

    void run();
    void timer();

    class CancelableTimer final : public Cancelable {
    public:
        CancelableTimer(
            const std::shared_ptr<SingleThreadScheduler>& parent,
            int64_t time_slot,
            int64_t id
        );

        void cancel() override;
        void onCancel(const std::function<void()>& callback) override;
    private:
        std::shared_ptr<SingleThreadScheduler> parent;
        int64_t time_slot;
        int64_t id;
        std::vector<std::function<void()>> callbacks;
        std::mutex callback_mutex;
        bool canceled;
    };

};

}

#endif