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
#include "SpinLock.hpp"

namespace cask::scheduler {
    
/**
 * The single thread scheduler only utilizes a single thread for processing submitted
 * work. It uses simpler lock-free protections where possible to protect its internal
 * structures. The result is a scheduler that is more optimized for applications where
 * the need for computational span across cores is not as strong. The benefit is greater
 * cache coherency and less time interacting with the OS for things like mutexes resulting
 * in faster handling of asynchrounous boundaries. This comes at the expensive of some amount
 * of fairness (because of spinlocks) and the inability to spread load across multiple cores.
 */
class SingleThreadScheduler final : public Scheduler, public std::enable_shared_from_this<SingleThreadScheduler> {
public:
    /**
     * Construct a single threaded scheduler.
     */
    explicit SingleThreadScheduler(int priority = 0);

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

    std::atomic_bool should_run;
    std::atomic_bool idle;
    std::atomic_bool timer_running;
    std::atomic_bool runner_running;

    std::condition_variable dataInQueue;

    mutable std::mutex idlingThreadMutex;
    mutable std::condition_variable idlingThread;
    mutable std::atomic_size_t readyQueueSize;
    mutable SpinLock readyQueueLock;
    mutable SpinLock timerLock;
    std::queue<std::function<void()>> readyQueue;
    std::map<int64_t,std::vector<TimerEntry>> timers;
    int64_t last_execution_ms;
    std::atomic_int64_t next_id;

    void run();
    static int64_t current_time_ms();

    class CancelableTimer final : public Cancelable {
    public:
        CancelableTimer(
            const std::shared_ptr<SingleThreadScheduler>& parent,
            int64_t time_slot,
            int64_t id
        );

        void cancel() override;
        void onCancel(const std::function<void()>& callback) override;
        void onShutdown(const std::function<void()>&) override;
    private:
        std::shared_ptr<SingleThreadScheduler> parent;
        int64_t time_slot;
        int64_t id;
        std::vector<std::function<void()>> callbacks;
        std::mutex callback_mutex;
        bool canceled;
    };

};

} // namespace cask::scheduler

#endif