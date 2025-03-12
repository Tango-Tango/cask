//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_TEST_BENCH_SCHEDULER_H_
#define _CASK_TEST_BENCH_SCHEDULER_H_

#include "../Scheduler.hpp"
#include <memory>
#include <mutex>
#include <tuple>
#include <vector>
#include <queue>

namespace cask::scheduler {

/**
 * The BenchScheduler is a scheduler that is specifically geared towards
 * making certain types of testing easier. To that end it supports a few
 * features:
 * 
 * 1. Tasks don't run in the background. Instead the scheduler must be
 *    manually pumped for execution via the `run_one_task` and `run_ready_tasks`
 *    methods.
 * 2. Timers don't automatically invoke. Instead the test bench must use the
 *    `advance_time` method to trigger timer tasks to become ready and then
 *    their execution must be manually pumped via the `run_one_task` and
 *    `run_ready_tasks` methods.
 * 
 * This gives the test bench a huge amount of control about when things
 * happen - and allows testing of code with timers in a repeatable and
 * controllable way.
 * 
 * This also puts the burden of actually running tasks as they become
 * available on the test bench. In general the normal global scheduler
 * is fine for testing unless your test bench finds itself needing this
 * control (e.g. because the unit under test has timers). In such a case
 * this scheduler implementation can be very handy.
 */
class BenchScheduler final : public Scheduler, public std::enable_shared_from_this<BenchScheduler> {
public:
    BenchScheduler();

    /**
     * Check the number of tasks that are currently
     * ready for execution.
     * 
     * @return The number of ready tasks.
     */
    std::size_t num_task_ready() const;

    /**
     * Check the current number of timers currently
     * scheduled for future execution in the timer list.
     * 
     * @return The number of timers.
     */
    std::size_t num_timers() const;

    /**
     * Run a single task from the ready queue.
     * 
     * @return True iff a task was executed.
     */
    bool run_one_task();

    /**
     * Run all tasks from the ready queue - including
     * tasks which may be scheduled for immediate execution
     * by the executing code. As a result the number of
     * tasks executed may be larger than what `num_tasks_ready()`
     * indicates before calling this method.
     * 
     * @return The number of tasks that were executed.
     */
    std::size_t run_ready_tasks();

    /**
     * Advance time by the given number of milliseconds. If advancing
     * time causes a timer's execution time to pass then the associated
     * task will be moved to the ready queue but not immediately executed.
     * Call `run_one_task` or `run_ready_tasks` to execute those timer
     * tasks.
     * 
     * @param milliseconds The number of milliseconds to advance time by.
     */
    void advance_time(int64_t milliseconds);

    bool submit(const std::function<void()>& task) override;
    bool submitBulk(const std::vector<std::function<void()>>& tasks) override;
    CancelableRef submitAfter(int64_t milliseconds, const std::function<void()>& task) override;
    bool isIdle() const override;
    std::string toString() const override;

private:
    using TimerEntry = std::tuple<int64_t, int64_t, std::function<void()>>;

    int64_t current_time;
    int64_t next_id;
    std::queue<std::function<void()>> ready_queue;
    std::vector<TimerEntry> timers;
    mutable std::mutex scheduler_mutex;

    class BenchCancelableTimer final : public Cancelable {
    public:
        BenchCancelableTimer(
            const std::shared_ptr<BenchScheduler>& parent,
            int64_t id
        );

        void cancel() override;
        void onCancel(const std::function<void()>& callback) override;
        void onShutdown(const std::function<void()>&) override;
    private:
        std::shared_ptr<BenchScheduler> parent;
        int64_t id;
        std::vector<std::function<void()>> callbacks;
        std::mutex callback_mutex;
        bool canceled;
    };
};

} // namespace cask::scheduler

#endif