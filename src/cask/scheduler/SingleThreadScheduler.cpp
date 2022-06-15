//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/scheduler/SingleThreadScheduler.hpp"
#include <chrono>

#if __linux__
#include <sys/resource.h>
#elif __APPLE__
#include <pthread.h>
#include <sys/resource.h>
#endif

using namespace std::chrono_literals;

namespace cask::scheduler {

SingleThreadScheduler::SingleThreadScheduler(int priority)
    : should_run(true)
    , idle(true)
    , timer_running(false)
    , runner_running(false)
    , dataInQueue()
    , readyQueueSize(0)
    , readyQueue()
    , timerMutex()
    , timers()
    , ticks(0)
{
    std::thread runThread(std::bind(&SingleThreadScheduler::run, this));
    std::thread timerThread(std::bind(&SingleThreadScheduler::timer, this));

#if __linux__
    auto which = PRIO_PROCESS;
    setpriority(which, runThread.native_handle(), priority);
    setpriority(which, timerThread.native_handle(), priority);
#elif __APPLE__
    auto which = PRIO_PROCESS;
    uint64_t run_thread_id = 0;
    uint64_t timer_thread_id = 0;

    pthread_threadid_np(runThread.native_handle(), &run_thread_id);
    pthread_threadid_np(timerThread.native_handle(), &timer_thread_id);

    setpriority(which, run_thread_id, priority);
    setpriority(which, timer_thread_id, priority);
#endif

    runThread.detach();
    timerThread.detach();

    while(!runner_running.load(std::memory_order_relaxed));
    while(!timer_running.load(std::memory_order_relaxed));
}

SingleThreadScheduler::~SingleThreadScheduler() {
    should_run.store(false);

    while(runner_running.load(std::memory_order_relaxed));
    while(timer_running.load(std::memory_order_relaxed));
}

void SingleThreadScheduler::submit(const std::function<void()>& task) {
    while(readyQueueLock.test_and_set(std::memory_order_acquire));
    readyQueue.emplace(task);
    readyQueueSize.fetch_add(1, std::memory_order_relaxed);
    readyQueueLock.clear(std::memory_order_release);
    idlingThread.notify_one();
}

void SingleThreadScheduler::submitBulk(const std::vector<std::function<void()>>& tasks) {
    while(readyQueueLock.test_and_set(std::memory_order_acquire));
    for(auto& task: tasks) {
        readyQueue.emplace(task);
    }
    readyQueueSize.fetch_add(tasks.size(), std::memory_order_relaxed);
    readyQueueLock.clear(std::memory_order_release);
    idlingThread.notify_one();
}

CancelableRef SingleThreadScheduler::submitAfter(int64_t milliseconds, const std::function<void()>& task) {
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

bool SingleThreadScheduler::isIdle() const {
    return idle && readyQueueSize.load() == 0;
}

void SingleThreadScheduler::run() {
    std::function<void()> task;

    runner_running.store(true, std::memory_order_relaxed);

    while(should_run.load(std::memory_order_relaxed)) {
        if(readyQueueSize.load(std::memory_order_relaxed) == 0) {
            idle = true;
            std::unique_lock<std::mutex> lock(idlingThreadMutex);
            idlingThread.wait_for(lock, 1ms, [this]{ return readyQueueSize.load(std::memory_order_relaxed) != 0 || !should_run.load(std::memory_order_relaxed); });
        } else {
            idle = false;
            while(readyQueueLock.test_and_set(std::memory_order_acquire));
            task = readyQueue.front();
            readyQueue.pop();
            readyQueueSize.fetch_sub(1, std::memory_order_relaxed);
            readyQueueLock.clear(std::memory_order_release);
            task();
        }
    }

    runner_running.store(false, std::memory_order_relaxed);
}

void SingleThreadScheduler::timer() {
    const static std::chrono::milliseconds sleep_time(10);

    timer_running.store(true, std::memory_order_relaxed);

    while(should_run.load(std::memory_order_relaxed)) {
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

    timer_running.store(false, std::memory_order_relaxed);
}

SingleThreadScheduler::CancelableTimer::CancelableTimer(
    const std::shared_ptr<SingleThreadScheduler>& parent,
    int64_t time_slot,
    int64_t id
)   : parent(parent)
    , time_slot(time_slot)
    , id(id)
    , callbacks()
    , callback_mutex()
    , canceled(false)
{}

void SingleThreadScheduler::CancelableTimer::cancel() {
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

void SingleThreadScheduler::CancelableTimer::onCancel(const std::function<void()>& callback) {
    if(canceled) {
        callback();
    } else {
        std::lock_guard<std::mutex> guard(callback_mutex);
        callbacks.emplace_back(callback);
    }
}

void SingleThreadScheduler::CancelableTimer::onShutdown(const std::function<void()>& shutdownCallback) {
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

} // namespace cask::scheduler