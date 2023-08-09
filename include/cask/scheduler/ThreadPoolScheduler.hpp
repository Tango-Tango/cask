//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_THREAD_POOL_SCHEDULER_H_
#define _CASK_THREAD_POOL_SCHEDULER_H_

#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>
#include <queue>
#include <vector>

#include "../Scheduler.hpp"

namespace cask::scheduler {

class ThreadPoolScheduler final : public Scheduler, public std::enable_shared_from_this<ThreadPoolScheduler> {
public:
    /**
     * Construct a scheduler optionally configuring the maximum number of threads
     * to use.
     * 
     * @param poolSize The number of threads to use - defaults to matching
     *                 the number of hardware threads available in the system.
     */
    explicit ThreadPoolScheduler(unsigned int poolSize = std::thread::hardware_concurrency());

    /**
     * Destruct the scheduler. Destruction waits for all running and timer
     * threads to stop before finishing.
     */
    ~ThreadPoolScheduler();

    void submit(const std::function<void()>& task) override;
    void submitBulk(const std::vector<std::function<void()>>& tasks) override;
    CancelableRef submitAfter(int64_t milliseconds, const std::function<void()>& task) override;
    bool isIdle() const override;
private:
    using TimerEntry = std::tuple<int64_t, std::function<void()>>;

    std::atomic_bool should_run;

    std::mutex readyQueueMutex;
    std::condition_variable dataInQueue;
    std::queue<std::function<void()>> readyQueue;
    std::atomic_size_t idleThreads;
    std::mutex timerMutex;
    std::condition_variable timerCondition;
    std::map<int64_t,std::vector<TimerEntry>> timers;
    std::vector<std::atomic_bool*> threadStatus;
    std::atomic_bool timerThreadStatus;
    std::atomic_int64_t next_id;
    int64_t last_execution_ms;

    void run(unsigned int thread_index);
    void timer();
    static int64_t current_time_ms();

    class CancelableTimer final : public Cancelable {
    public:
        CancelableTimer(
            const std::shared_ptr<ThreadPoolScheduler>& parent,
            int64_t time_slot,
            int64_t id
        );

        void cancel() override;
        void onCancel(const std::function<void()>& callback) override;
        void onShutdown(const std::function<void()>& callback) override;
    private:
        std::shared_ptr<ThreadPoolScheduler> parent;
        int64_t time_slot;
        int64_t id;
        std::vector<std::function<void()>> callbacks;
        std::mutex callback_mutex;
        bool canceled;
    };
};

} // namespace cask::scheduler

#endif
