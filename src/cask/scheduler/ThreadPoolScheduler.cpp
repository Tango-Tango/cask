//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/scheduler/ThreadPoolScheduler.hpp"
#include <chrono>

namespace cask::scheduler {

ThreadPoolScheduler::ThreadPoolScheduler(int poolSize)
    : running(true)
    , readyQueueMutex()
    , dataInQueue()
    , readyQueue()
    , idleThreads(poolSize)
    , timerMutex()
    , timers()
    , runThreads()
    , timerThread()
    , ticks(0)
{
    for(int i =0; i < poolSize; i++) {
        std::thread poolThread(std::bind(&ThreadPoolScheduler::run, this));
        runThreads.push_back(std::move(poolThread));
    }

    timerThread = std::thread(std::bind(&ThreadPoolScheduler::timer, this));
}

ThreadPoolScheduler::~ThreadPoolScheduler() {
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

void ThreadPoolScheduler::submit(const std::function<void()>& task) {
    {
        std::lock_guard<std::mutex> guard(readyQueueMutex);
        readyQueue.emplace(task);
    }
    dataInQueue.notify_one();
}

void ThreadPoolScheduler::submitBulk(const std::vector<std::function<void()>>& tasks) {
    std::lock_guard<std::mutex> guard(readyQueueMutex);
    for(auto& task: tasks) {
        readyQueue.emplace(task);
        dataInQueue.notify_one();
    }
}

CancelableRef ThreadPoolScheduler::submitAfter(int64_t milliseconds, const std::function<void()>& task) {
    std::lock_guard<std::mutex> guard(timerMutex);
    int64_t executionTick = ticks + milliseconds;

    auto id = next_id++;
    auto tasks = timers.find(executionTick);
    auto entry = std::make_tuple(id, task);
    auto cancelable = std::make_shared<CancelableTimer>(
        this->shared_from_this(),
        executionTick,
        id
    );

    if(tasks == timers.end()) {
        std::vector<TimerEntry> taskVector = {entry};
        timers[executionTick] = taskVector;
    } else {
        tasks->second.push_back(entry);
    }

    return cancelable;
}

bool ThreadPoolScheduler::isIdle() const {
    return idleThreads.load() == runThreads.size() && readyQueue.empty();
}

void ThreadPoolScheduler::run() {
    std::unique_lock<std::mutex> readyQueueLock(readyQueueMutex, std::defer_lock);
    std::chrono::milliseconds max_wait_time(10);
    std::function<void()> task;
    bool idling = true;

    while(running) {
        readyQueueLock.lock();

        if(!idling && readyQueue.empty()) {
            idling = true;
            idleThreads++;
        }

        if(dataInQueue.wait_for(readyQueueLock, max_wait_time, [this](){return !readyQueue.empty(); })) {
            if(idling) {
                idling = false;
                idleThreads--;
            }
            
            task = readyQueue.front();
            readyQueue.pop();
            readyQueueLock.unlock();
            task();
        } else {
            readyQueueLock.unlock();
        }
    }
}

void ThreadPoolScheduler::timer() {
    const static std::chrono::milliseconds sleep_time(10);

    while(running) {
        auto before = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(sleep_time);
        auto after = std::chrono::steady_clock::now();
        
        auto delta = after - before;
        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(delta).count();

        {
            std::lock_guard<std::mutex> guard(timerMutex);

            int64_t previous_ticks = ticks;
            ticks += milliseconds;

            for(int64_t i = previous_ticks; i <= ticks; i++) {
                auto tasks = timers.find(i);
                if(tasks != timers.end()) {
                    std::vector<std::function<void()>> submittedTasks;
                    auto entries = tasks->second;

                    submittedTasks.reserve(entries.size());

                    for(auto& entry : entries) {
                        submittedTasks.emplace_back(std::get<1>(entry));
                    }

                    submitBulk(submittedTasks);
                    timers.erase(i);
                }
            }
        }
    }
}

ThreadPoolScheduler::CancelableTimer::CancelableTimer(
    const std::shared_ptr<ThreadPoolScheduler>& parent,
    int64_t time_slot,
    int64_t id
)   : parent(parent)
    , time_slot(time_slot)
    , id(id)
    , callbacks()
    , callback_mutex()
    , canceled(false)
{}

void ThreadPoolScheduler::CancelableTimer::cancel() {
    if(canceled) {
        return;
    } else {
        std::lock_guard<std::mutex> parent_guard(parent->timerMutex);
        std::lock_guard<std::mutex> self_guard(callback_mutex);
        auto tasks = parent->timers.find(time_slot);
        if(tasks != parent->timers.end()) {
            std::vector<TimerEntry> filteredEntries;
            auto entries = tasks->second;

            for(auto& entry : entries) {
                auto entry_id = std::get<0>(entry);
                if(entry_id != id) {
                    filteredEntries.emplace_back(entry);
                } else {
                    canceled = true;
                }
            }

            if(filteredEntries.size() > 0) {
                parent->timers[time_slot] = filteredEntries;
            } else {
                parent->timers.erase(time_slot);
            }
        }
    }

    if(canceled) {
        for(auto& cb : callbacks) {
            cb();
        }
    }
}

int ThreadPoolScheduler::CancelableTimer::onCancel(const std::function<void()>& callback) {
    if(canceled) {
        callback();
    } else {
        std::lock_guard<std::mutex> guard(callback_mutex);
        callbacks.emplace_back(callback);
    }

    return 0;
}

void ThreadPoolScheduler::CancelableTimer::onShutdown(const std::function<void()>& shutdownCallback) {
    bool found = false;

    {
        std::lock_guard<std::mutex> parent_guard(parent->timerMutex);
        std::lock_guard<std::mutex> self_guard(callback_mutex);
        auto tasks = parent->timers.find(time_slot);

        if(tasks != parent->timers.end()) {
            std::vector<TimerEntry> newEntries;
            auto entries = tasks->second;

            for(auto& entry : entries) {
                auto entryId = std::get<0>(entry);
                auto entryCallback = std::get<1>(entry);
                if(entryId != id) {
                    newEntries.emplace_back(entry);
                } else {
                    found = true;
                    newEntries.emplace_back(entryId, [entryCallback, shutdownCallback]() {
                        entryCallback();
                        shutdownCallback();
                    });
                }
            }

            parent->timers[time_slot] = newEntries;
        }
    }

    if(!found) {
        shutdownCallback();
    }
}

void ThreadPoolScheduler::CancelableTimer::unregisterCancelCallback(int) {
    
}

} // namespace cask::scheduler