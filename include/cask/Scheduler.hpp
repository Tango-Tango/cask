//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_SCHEDULER_H_
#define _CASK_SCHEDULER_H_

#include <functional>
#include <memory>

namespace cask {

class Scheduler;
using SchedulerRef = std::shared_ptr<Scheduler>;

/**
 * A Scheduler represents a thread pool upon which asynchronous operations
 * may execute their individual computation tasks.
 */
class Scheduler {
public:
    /**
     * Obtain a reference to the global default scheduler.
     *
     * @return The default globally available scheduler instance.
     */
    static SchedulerRef global();

    /**
     * Submit a task for execution in the thread pool. This task will
     * execute after an indeterminite amount of time as resources free
     * to perform the task.
     * 
     * @param task The task to submit for execution.
     */
    virtual void submit(const std::function<void()>& task) = 0;

    /**
     * Submit several tasks at once to the the thread pool. The order
     * these tasks will be taken up and executed is undefined. Each
     * task will execute after an indeterminite amount of time as
     * resource free to perform the individual task.
     * 
     * @param tasks The vector of tasks to submit in-bulk.
     */
    virtual void submitBulk(const std::vector<std::function<void()>>& tasks) = 0;

    /**
     * Submit a task to the pool after _at least_ the given amount
     * of time has passed.
     * 
     * @param milliseconds The number of milliseconds to wait before
     *                     submitting to the pool
     * @param task The task the submit after the wait time has elapsed.
     */
    virtual void submitAfter(int64_t milliseconds, const std::function<void()>& task) = 0;

    /**
     * Check if the scheduler is currently idle - meaning all threads are
     * currently waiting for tasks to execute.
     * 
     * @return true if the scheduler is idle.
     */
    virtual bool isIdle() const = 0;

    virtual ~Scheduler() = default;
};

}

#endif
