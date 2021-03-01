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
    , readyQueue()
    , timerMutex()
    , timers()
    , runThreads()
    , producerTokens()
    , timerThread()
{
    for(int i =0; i < poolSize; i++) {
        std::thread poolThread(std::bind(&Scheduler::run, this));
        producerTokens.emplace(poolThread.get_id(), moodycamel::ProducerToken(readyQueue));
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

void Scheduler::submit(std::function<void()> task) {
    auto tokenIter = producerTokens.find(std::this_thread::get_id());

    if(tokenIter != producerTokens.end()) {
        readyQueue.enqueue(tokenIter->second, task);
    } else {
        readyQueue.enqueue(task);
    }
}

void Scheduler::submitAfter(int milliseconds, std::function<void()> task) {
    long executionTick = ticks.load() + milliseconds;
    {
        std::lock_guard guard(timerMutex);
        auto tasks = timers.find(executionTick);
        if(tasks == timers.end()) {
            std::vector<std::function<void()>> taskVector = {task};
            timers[executionTick] = taskVector;
        } else {
            tasks->second.push_back(task);
        }
    }
}

void Scheduler::run() {
    moodycamel::ConsumerToken token(readyQueue);
    std::function<void()> task;
    while(running) {
        if(readyQueue.wait_dequeue_timed(token, task, std::chrono::milliseconds(1))) {
            task();
        }
    }
}

void Scheduler::timer() {
    while(running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        long currentTick = ticks.fetch_add(1);

        {
            std::lock_guard guard(timerMutex);
            auto tasks = timers.find(currentTick);
            if(tasks != timers.end()) {
                for(auto& task: tasks->second) {
                    submit(task);
                }
                timers.erase(currentTick);
            }
        }
    }
}

}