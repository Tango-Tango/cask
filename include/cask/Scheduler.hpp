//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_SCHEDULER_H_
#define _CASK_SCHEDULER_H_

#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>
#include "blockingconcurrentqueue.h"

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
    void submit(std::function<void()> task);

    /**
     * Submit a task to the pool after _at least_ the given amount
     * of time has passed.
     * 
     * @param milliseconds The number of milliseconds to wait before
     *                     submitting to the pool
     * @param task The task the submit after the wait time has elapsed.
     */
    void submitAfter(int milliseconds, std::function<void()> task);
private:
    bool running;
    moodycamel::BlockingConcurrentQueue<std::function<void()>> readyQueue;
    std::mutex timerMutex;
    std::map<long,std::vector<std::function<void()>>> timers;
    std::vector<std::thread> runThreads;
    std::map<std::thread::id,moodycamel::ProducerToken> producerTokens;
    std::thread timerThread;
    std::atomic_long ticks;

    void run();
    void timer();
};

}

#endif
