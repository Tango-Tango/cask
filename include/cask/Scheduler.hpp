//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_SCHEDULER_H_
#define _CASK_SCHEDULER_H_

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>
#include <queue>

namespace cask {

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
    static std::shared_ptr<Scheduler> global();

    /**
     * Construct a scheduler optionally configuring the maximum number of threads
     * to use.
     * 
     * @param poolSize The number of threads to use - defaults to matching
     *                 the number of hardware threads available in the system.
     */
    Scheduler(int poolSize = std::thread::hardware_concurrency());

    /**
     * Destruct the scheduler. Destruction waits for all running and timer
     * threads to stop before finishing.
     */
    ~Scheduler();

    /**
     * Submit a task for execution in the thread pool. This task will
     * execute after an indeterminite amount of time as resources free
     * to perform the task.
     * 
     * @param task The task to submit for execution.
     */
    void submit(const std::function<void()>& task);

    /**
     * Submit several tasks at once to the the thread pool. The order
     * these tasks will be taken up and executed is undefined. Each
     * task will execute after an indeterminite amount of time as
     * resource free to perform the individual task.
     * 
     * @param tasks The vector of tasks to submit in-bulk.
     */
    void submitBulk(const std::vector<std::function<void()>>& tasks);

    /**
     * Submit a task to the pool after _at least_ the given amount
     * of time has passed.
     * 
     * @param milliseconds The number of milliseconds to wait before
     *                     submitting to the pool
     * @param task The task the submit after the wait time has elapsed.
     */
    void submitAfter(int64_t milliseconds, const std::function<void()>& task);
private:
    bool running;

    std::mutex readyQueueMutex;
    std::condition_variable dataInQueue;
    std::queue<std::function<void()>> readyQueue;
    std::mutex timerMutex;
    std::map<int64_t,std::vector<std::function<void()>>> timers;
    std::vector<std::thread> runThreads;
    std::thread timerThread;
    int64_t ticks;

    void run();
    void timer();
};

}

#endif
