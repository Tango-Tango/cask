//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/scheduler/SingleThreadScheduler.hpp"
#include <algorithm>
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
    , last_execution_ms(current_time_ms())
{
    std::thread runThread(std::bind(&SingleThreadScheduler::run, this));

#if __linux__
    auto which = PRIO_PROCESS;
    setpriority(which, runThread.native_handle(), priority);
    setpriority(which, timerThread.native_handle(), priority);
#elif __APPLE__
    auto which = PRIO_PROCESS;
    uint64_t run_thread_id = 0;
    uint64_t timer_thread_id = 0;

    pthread_threadid_np(runThread.native_handle(), &run_thread_id);

    setpriority(which, run_thread_id, priority);
    setpriority(which, timer_thread_id, priority);
#endif

#if defined(__linux__) && defined(_GNU_SOURCE)
    auto handle = runThread.native_handle();

    // NOLINTNEXTLINE(bugprone-narrowing-conversions)
    int core_id = rand() % std::thread::hardware_concurrency();

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset);
#endif

    runThread.detach();

    while(!runner_running.load(std::memory_order_relaxed));
}

SingleThreadScheduler::~SingleThreadScheduler() {
    should_run.store(false);
    idlingThread.notify_one();
    while(runner_running.load(std::memory_order_relaxed));
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
    auto id = next_id++;
    auto executionTick = current_time_ms() + milliseconds;

    {
        std::lock_guard<std::mutex> guard(timerMutex);

        auto tasks = timers.find(executionTick);
        auto entry = std::make_tuple(id, task);
        
        if(tasks == timers.end()) {
            std::vector<TimerEntry> taskVector = {entry};
            timers[executionTick] = taskVector;
        } else {
            tasks->second.push_back(entry);
        }
    }

    auto cancelable = std::make_shared<CancelableTimer>(
        this->shared_from_this(),
        executionTick,
        id
    );

    idlingThread.notify_one();

    return cancelable;
}

bool SingleThreadScheduler::isIdle() const {
    return idle && readyQueueSize.load() == 0;
}

void SingleThreadScheduler::run() {
    std::function<void()> task;

    auto dormant_sleep_time = std::chrono::milliseconds(1000);
    auto sleep_time = dormant_sleep_time;

    runner_running.store(true, std::memory_order_relaxed);

    while(should_run.load(std::memory_order_relaxed)) {
        auto current_time = current_time_ms();
        std::vector<int64_t> expiredTimes;
        std::vector<std::function<void()>> expiredTasks;

        // Accumulate any expired tasks
        {   
            std::lock_guard<std::mutex> guard(timerMutex);
            for (auto& [timer_time, entries] : timers) {
                if (timer_time <= current_time) {
                    expiredTasks.reserve(expiredTasks.size() + entries.size());

                    for(auto& entry : entries) {
                        expiredTasks.emplace_back(std::get<1>(entry));
                    }

                    expiredTimes.push_back(timer_time);
                } else {
                    break;
                }
            }

            // Erase those timers from timer storage
            for (auto& timerTime : expiredTimes) {
                timers.erase(timerTime);
            }
        }

        // Execute any expired timer tasks immediately
        for(auto& timerTask : expiredTasks) {
            timerTask();
        }

        // Drain the ready queue - being careful to observe if we should continue running
        // and capping the number of tasks we'll run in a single loop iteration
        auto task_batch_size = 128;

        while (readyQueueSize.load(std::memory_order_relaxed) > 0 && should_run.load(std::memory_order_relaxed) && task_batch_size-- > 0) {
            idle = false;
            while(readyQueueLock.test_and_set(std::memory_order_acquire));
            task = readyQueue.front();
            readyQueue.pop();
            readyQueueSize.fetch_sub(1, std::memory_order_relaxed);
            readyQueueLock.clear(std::memory_order_release);
            task();
        }

        // Compute the next sleep time based on the current time, the next timer, and
        // the number of remaining tasks in the ready queue
        if (readyQueueSize.load(std::memory_order_relaxed) > 0) {
            sleep_time = std::chrono::milliseconds(0);
        } else {
            std::lock_guard<std::mutex> guard(timerMutex);
            if (timers.empty()) {
                sleep_time = dormant_sleep_time;
            } else {
                auto next_timer = timers.begin();
                auto next_timer_time = next_timer->first;
                auto next_timer_sleep_time = std::chrono::milliseconds(next_timer_time - current_time);

                next_timer_sleep_time = std::min(next_timer_sleep_time, dormant_sleep_time);
                next_timer_sleep_time = std::max(next_timer_sleep_time, std::chrono::milliseconds(0));

                sleep_time = std::chrono::milliseconds(next_timer_sleep_time);
            }
        }

        // Idle the run thread only if we've detect that we should sleep for
        // some positive amount of time
        if (sleep_time > std::chrono::milliseconds(0)) {
            idle = true;
            std::unique_lock<std::mutex> lock(idlingThreadMutex);
            idlingThread.wait_for(lock, sleep_time);
        }
    }

    runner_running.store(false, std::memory_order_relaxed);
}

int64_t SingleThreadScheduler::current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
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