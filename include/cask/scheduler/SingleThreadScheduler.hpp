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
#include <optional>
#include <thread>
#include <queue>
#include <vector>

#include "../Scheduler.hpp"

namespace cask::scheduler {

constexpr std::size_t DEFAULT_BATCH_SIZE = 128;
    
/**
 * The single thread scheduler only utilizes a single thread for processing
 * submitted work.
 */
class SingleThreadScheduler final : public Scheduler, public std::enable_shared_from_this<SingleThreadScheduler> {
public:
    // NOLINTBEGIN(bugprone-easily-swappable-parameters)

    /**
     * Construct a single threaded scheduler.
     */
    explicit SingleThreadScheduler(
        std::optional<int> priority = std::nullopt,
        std::optional<int> pinned_core = std::nullopt,
        std::optional<std::size_t> batch_size = std::nullopt,
        const std::function<void()>& on_idle = [](){},
        const std::function<void()>& on_resume = [](){},
        const std::function<std::vector<std::function<void()>>(std::size_t)>& on_request_work = [](auto){ return std::vector<std::function<void()>>(); }
    );

    // NOLINTEND(bugprone-easily-swappable-parameters)

    /**
     * Destruct the scheduler. Destruction waits for all running and timer
     * threads to stop before finishing.
     */
    ~SingleThreadScheduler();

    /**
     * Determine the run thread's ID
     */
    std::thread::id get_run_thread_id() const;

    /**
     * Steal some work from this scheduler's ready queue.
     */
    std::vector<std::function<void()>> steal(std::size_t batch_size);

    void submit(const std::function<void()>& task) override;
    void submitBulk(const std::vector<std::function<void()>>& tasks) override;
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
            const std::function<std::vector<std::function<void()>>(std::size_t)>& on_request_work);
        // NOLINTEND(bugprone-easily-swappable-parameters)

        std::atomic_bool thread_running;

        std::mutex mutex;
        std::condition_variable work_available;

        bool should_run;
        bool idle;
        std::map<TimerTimeMs,std::vector<std::shared_ptr<CancelableTimer>>> timers;
        std::queue<std::function<void()>> ready_queue;

        std::function<void()> on_idle;
        std::function<void()> on_resume;
        std::function<std::vector<std::function<void()>>(std::size_t)> on_request_work;
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

    TimerId next_id;
    std::thread::id run_thread_id;
    std::shared_ptr<SchedulerControlData> control_data;

    static void run(const std::size_t batch_size, const std::shared_ptr<SchedulerControlData>& control_data);
    static std::size_t peek_available_work_unsafe(const std::shared_ptr<SchedulerControlData>& control_data);
    static std::vector<std::function<void()>> steal_unsafe(const std::shared_ptr<SchedulerControlData>& control_data, std::size_t batch_size);
    static int64_t current_time_ms();
};

} // namespace cask::scheduler

#endif