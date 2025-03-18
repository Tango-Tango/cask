//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_SINGLE_THREAD_SCHEDULER_H_
#define _CASK_SINGLE_THREAD_SCHEDULER_H_

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <thread>
#include <deque>
#include <vector>

#include "../Scheduler.hpp"
#include "ReadyQueue.hpp"
#include "ThreadStartBarrier.hpp"

namespace cask::scheduler {

constexpr std::size_t DEFAULT_BATCH_SIZE = 128;

enum AutoStart : std::uint8_t {
    EnableAutoStart,
    DisableAutoStart
};
    
/**
 * The single thread scheduler only utilizes a single thread for processing
 * submitted work.
 */
class SingleThreadScheduler final : public Scheduler, public std::enable_shared_from_this<SingleThreadScheduler> {
public:
    // NOLINTBEGIN(bugprone-easily-swappable-parameters)

    /**
     * Construct a single threaded scheduler.
     * 
     * NOTE: The callbacks provided here are useful for when this scheduler is subordinate
     *       to another scheduler (e.g. the WorkStealingScheduler). There should be no
     *       reason to set these callbacks otherwise.
     * 
     * @param priority The priority to set the run thread to. Defaults to not setting priority.
     * @param pinned_core The core to pin the run thread to. Defaults to not pinning the thread.
     * @param max_queue_size The maximum size of the ready queue. Defaults to no maximum.
     * @param on_idle A callback to run when the scheduler becomes idle.
     * @param on_resume A callback to run when the scheduler resumes from idle.
     * @param on_request_work A callback to run when the scheduler requests work.
     * @param on_work_overflow A callback to run when the scheduler's ready queue overflows internally.
     * @param auto_start Whether to automatically start the scheduler.
     */
    explicit SingleThreadScheduler(
        std::optional<int> priority = std::nullopt,
        std::optional<int> pinned_core = std::nullopt,
        std::optional<std::size_t> max_queue_size = std::nullopt,
        const std::function<void()>& on_idle = [](){},
        const std::function<void()>& on_resume = [](){},
        const std::function<void(ReadyQueue&)>& on_request_work = [](auto&) {},
        const std::function<void(std::deque<std::function<void()>>&)>& on_work_overflow = [](auto&) {},
        AutoStart auto_start = EnableAutoStart
    );

    // NOLINTEND(bugprone-easily-swappable-parameters)

    /**
     * Destruct the scheduler. Destruction waits for all running and timer
     * threads to stop before finishing.
     */
    ~SingleThreadScheduler();

    /**
     * Start the scheduler if it isn't already started.
     */
    void start();

    /**
     * Try and wake the scheduler if it is idle.
     */
    void try_wake();

    /**
     * Signal that the scheduler should be shuttin down.
     */
    void stop();

    /**
     * Determine the run thread's ID
     */
    std::thread::id get_run_thread_id() const;

    /**
     * Steal some work from this scheduler's ready queue.
     */
    bool steal(ReadyQueue& requestor_ready_queue);

    bool submit(const std::function<void()>& task) override;
    bool submitBulk(const std::vector<std::function<void()>>& tasks) override;
    CancelableRef submitAfter(int64_t milliseconds, const std::function<void()>& task) override;
    bool isIdle() const override;
    std::string toString() const override;
private:
    using TimerTimeMs = int64_t;
    using TimerId = int64_t;

    class CancelableTimer;
        
    struct SchedulerControlData {

        // NOLINTBEGIN(bugprone-easily-swappable-parameters)
        SchedulerControlData(
            const std::function<void()>& on_idle,
            const std::function<void()>& on_resume,
            const std::function<void(ReadyQueue&)>& on_request_work,
            const std::function<void(std::deque<std::function<void()>>&)>& on_work_overflow);
        // NOLINTEND(bugprone-easily-swappable-parameters)

        std::atomic_bool thread_running;
        ThreadStartBarrier start_barrier; 

        bool should_run;
        std::atomic_bool idle;
        ReadyQueue ready_queue;

        std::mutex timers_mutex;
        std::map<TimerTimeMs,std::vector<std::shared_ptr<CancelableTimer>>> timers;

        std::function<void()> on_idle;
        std::function<void()> on_resume;
        std::function<void(ReadyQueue&)> on_request_work;
        std::function<void(std::deque<std::function<void()>>&)> on_work_overflow;
    };

    class CancelableTimer final : public Cancelable, std::enable_shared_from_this<CancelableTimer> {
    public:

        // NOLINTBEGIN(bugprone-easily-swappable-parameters)

        CancelableTimer(
            const std::shared_ptr<SchedulerControlData>& control_data,
            TimerTimeMs time_slot,
            TimerId id
        );

        // NOLINTEND(bugprone-easily-swappable-parameters)

        void fire();
        void cancel() override;
        void onCancel(const std::function<void()>& callback) override;
        void onShutdown(const std::function<void()>&) override;
    private:

        std::shared_ptr<SchedulerControlData> control_data;
        TimerTimeMs time_slot;
        TimerId id;
        std::vector<std::function<void()>> shutdown_callbacks;
        std::vector<std::function<void()>> cancel_callbacks;
        std::mutex timer_mutex;
        bool canceled;
        bool shutdown;
    };

    const std::optional<int> priority;
    const std::optional<int> pinned_core;
    const std::size_t max_queue_size;
    
    bool started;
    TimerId next_id;
    std::thread::id run_thread_id;
    std::shared_ptr<SchedulerControlData> control_data;

    static void run(const std::shared_ptr<SchedulerControlData>& control_data);
    static std::deque<std::function<void()>> evaluate_timers_unsafe(const std::shared_ptr<SchedulerControlData>& control_data);
    static bool steal_unsafe(const std::shared_ptr<SchedulerControlData>& control_data, std::deque<std::function<void()>>& requestor_ready_queue, std::size_t requested_amount);
    static int64_t current_time_ms();
};

} // namespace cask::scheduler

#endif