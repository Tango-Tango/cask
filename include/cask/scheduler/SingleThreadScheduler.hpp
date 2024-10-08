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


    
/**
 * The single thread scheduler only utilizes a single thread for processing
 * submitted work.
 */
class SingleThreadScheduler final : public Scheduler, public std::enable_shared_from_this<SingleThreadScheduler> {
public:
    /**
     * Construct a single threaded scheduler.
     */
    explicit SingleThreadScheduler(
        std::optional<int> priority = std::nullopt,
        std::optional<int> pinned_core = std::nullopt,
        const std::function<void()>& on_idle = [](){},
        const std::function<void()>& on_resume = [](){},
        const std::function<std::vector<std::function<void()>>()>& on_request_work = [](){ return std::vector<std::function<void()>>(); }
    );

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
    using TimerEntry = std::tuple<int64_t, std::function<void()>>;

    struct SchedulerControlData {
        SchedulerControlData(
            const std::function<void()>& on_idle,
            const std::function<void()>& on_resume,
            const std::function<std::vector<std::function<void()>>()>& on_request_work);

        std::atomic_bool thread_running;

        std::mutex mutex;
        std::condition_variable work_available;

        bool should_run;
        bool idle;
        std::map<int64_t,std::vector<TimerEntry>> timers;
        std::queue<std::function<void()>> ready_queue;

        std::function<void()> on_idle;
        std::function<void()> on_resume;
        std::function<std::vector<std::function<void()>>()> on_request_work;
    };

    class CancelableTimer final : public Cancelable {
    public:
        CancelableTimer(
            const std::shared_ptr<SchedulerControlData>& control_data,
            int64_t time_slot,
            int64_t id
        );

        void cancel() override;
        void onCancel(const std::function<void()>& callback) override;
        void onShutdown(const std::function<void()>&) override;
    private:
        std::shared_ptr<SchedulerControlData> control_data;
        int64_t time_slot;
        int64_t id;
        std::vector<std::function<void()>> callbacks;
        std::mutex callback_mutex;
        bool canceled;
    };

    std::int64_t next_id;
    std::thread::id run_thread_id;
    std::shared_ptr<SchedulerControlData> control_data;

    static void run(const std::shared_ptr<SchedulerControlData>& control_data);
    static int64_t current_time_ms();
};

} // namespace cask::scheduler

#endif