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

class ThreadPoolScheduler final : public Scheduler {
public:
    /**
     * Construct a scheduler optionally configuring the maximum number of threads
     * to use.
     * 
     * @param poolSize The number of threads to use - defaults to matching
     *                 the number of hardware threads available in the system.
     */
    ThreadPoolScheduler(int poolSize = std::thread::hardware_concurrency());

    /**
     * Destruct the scheduler. Destruction waits for all running and timer
     * threads to stop before finishing.
     */
    ~ThreadPoolScheduler();

    void submit(const std::function<void()>& task) override;
    void submitBulk(const std::vector<std::function<void()>>& tasks) override;
    void submitAfter(int64_t milliseconds, const std::function<void()>& task) override;
    bool isIdle() const override;
private:
    bool running;

    std::mutex readyQueueMutex;
    std::condition_variable dataInQueue;
    std::queue<std::function<void()>> readyQueue;
    std::atomic_size_t idleThreads;
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
