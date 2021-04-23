//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/Scheduler.hpp"
#include <chrono>

namespace cask {


std::shared_ptr<Scheduler> Scheduler::global() {
    static std::shared_ptr<Scheduler> sched = std::make_shared<Scheduler>();
    return sched;
}

Scheduler::Scheduler(int poolSize)
    : running(true)
    , readyQueueMutex()
    , dataInQueue()
    , readyQueue()
    , timerMutex()
    , timers()
    , runThreads()
    , timerThread()
    , ticks(0)
{
    for(int i =0; i < poolSize; i++) {
        std::thread poolThread(std::bind(&Scheduler::run, this));
        runThreads.push_back(std::move(poolThread));
    }

    timerThread = std::thread(std::bind(&Scheduler::timer, this));
}

Scheduler::~Scheduler() {
    running = false;

    for(auto& thread: runThreads) {
        try {
            thread.join();
        } catch(const std::system_error& error) {}
    }

    try {
        timerThread.join();
    } catch(const std::system_error& error) {}
}

void Scheduler::submit(const std::function<void()>& task) {
    {
        std::lock_guard guard(readyQueueMutex);
        readyQueue.emplace(task);
    }
    dataInQueue.notify_one();
}

void Scheduler::submitBulk(const std::vector<std::function<void()>>& tasks) {
    std::lock_guard guard(readyQueueMutex);
    for(auto& task: tasks) {
        readyQueue.emplace(task);
        dataInQueue.notify_one();
    }
}

void Scheduler::submitAfter(int64_t milliseconds, const std::function<void()>& task) {
    std::lock_guard guard(timerMutex);
    int64_t executionTick = ticks + milliseconds;
    auto tasks = timers.find(executionTick);
    if(tasks == timers.end()) {
        std::vector<std::function<void()>> taskVector = {task};
        timers[executionTick] = taskVector;
    } else {
        tasks->second.push_back(task);
    }
}

void Scheduler::run() {
    std::unique_lock<std::mutex> readyQueueLock(readyQueueMutex, std::defer_lock);
    std::chrono::milliseconds max_wait_time(10);
    std::function<void()> task;
    while(running) {
        readyQueueLock.lock();
        if(dataInQueue.wait_for(readyQueueLock, max_wait_time, [this](){return !readyQueue.empty(); })) {
            task = readyQueue.front();
            readyQueue.pop();
            readyQueueLock.unlock();
            task();
        } else {
            readyQueueLock.unlock();
        }
    }
}

void Scheduler::timer() {
    const static std::chrono::milliseconds sleep_time(10);

    while(running) {
        auto before = std::chrono::high_resolution_clock::now();
        std::this_thread::sleep_for(sleep_time);
        auto after = std::chrono::high_resolution_clock::now();
        
        auto delta = after - before;
        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(delta).count();

        {
            std::lock_guard guard(timerMutex);

            int64_t previous_ticks = ticks;
            ticks += milliseconds;

            for(int64_t i = previous_ticks; i <= ticks; i++) {
                auto tasks = timers.find(i);
                if(tasks != timers.end()) {
                    submitBulk(tasks->second);
                    timers.erase(i);
                }
            }
        }
    }
}

} // namespace cask