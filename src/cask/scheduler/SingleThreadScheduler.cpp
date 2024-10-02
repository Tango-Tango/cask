//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/scheduler/SingleThreadScheduler.hpp"
#include "cask/scheduler/SpinLock.hpp"
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

SingleThreadScheduler::SingleThreadScheduler(
    int priority,
    std::function<void(std::shared_ptr<SingleThreadScheduler>)> on_idle,
    std::function<void(std::shared_ptr<SingleThreadScheduler>)> on_resume)
    : on_idle(on_idle)
    , on_resume(on_resume)
    , should_run(true)
    , idle(true)
    , runner_running(false)
    , dataInQueue()
    , readyQueueSize(0)
    , readyQueue()
    , timers()
    , last_execution_ms(current_time_ms())
{
    std::thread runThread(std::bind(&SingleThreadScheduler::run, this));

#if __linux__
    auto which = PRIO_PROCESS;
    setpriority(which, runThread.native_handle(), priority);
#elif __APPLE__
    auto which = PRIO_PROCESS;
    uint64_t run_thread_id = 0;

    pthread_threadid_np(runThread.native_handle(), &run_thread_id);

    setpriority(which, run_thread_id, priority);
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

    runThreadId = runThread.get_id();
    runThread.detach();

    // Wait for the run thread to start. Use a relaxed fence since we're
    // just waiting for the thread to start and it's fine if the value
    // takes some time to propagate.
    while(!runner_running.load(std::memory_order_relaxed));
}

SingleThreadScheduler::~SingleThreadScheduler() {
    should_run.store(false);
    idlingThread.notify_one();


    // Wait for the run thread to stop. Use a relaxed fence since we're
    // just waiting for the thread to stop and it's fine if the value
    // takes some time to propagate.
    while(runner_running.load(std::memory_order_relaxed));
}

std::thread::id SingleThreadScheduler::run_thread_id() const {
    return runThreadId;
}

std::vector<std::function<void()>> SingleThreadScheduler::steal(std::size_t batch_size) {
    std::function<void()> task;
    std::vector<std::function<void()>> batch;

    {
        SpinLockGuard guard(readyQueueLock);
        auto queueSize = readyQueueSize.load(std::memory_order_relaxed);
        auto batchSize = std::min(queueSize, batch_size);
        batch.reserve(batchSize);

        while(batch.size() < batchSize) {
            task = readyQueue.front();
            readyQueue.pop();
            batch.emplace_back(task);
        }
    }

    return batch;
}

void SingleThreadScheduler::submit(const std::function<void()>& task) {
    {
        SpinLockGuard guard(readyQueueLock);
        readyQueue.emplace(task);

        // Increment the ready queue size with relaxed memory ordering
        // since the SpinLockGuard above provides the necessary release
        // fence
        readyQueueSize.fetch_add(1, std::memory_order_relaxed);
    }
    idlingThread.notify_one();
}

void SingleThreadScheduler::submitBulk(const std::vector<std::function<void()>>& tasks) {
    {
        SpinLockGuard guard(readyQueueLock);
        for(auto& task: tasks) {
            readyQueue.emplace(task);
        }

        // Increment the ready queue size with relaxed memory ordering
        // since the SpinLockGuard above provides the necessary release
        // fence
        readyQueueSize.fetch_add(tasks.size(), std::memory_order_relaxed);
    }
    idlingThread.notify_one();
}

CancelableRef SingleThreadScheduler::submitAfter(int64_t milliseconds, const std::function<void()>& task) {
    auto id = next_id++;
    auto executionTick = current_time_ms() + milliseconds;

    {
        SpinLockGuard guard(timerLock);
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

    // Using relaxed fences here since the values of these flags
    // can be stale so long as they eventually (within a few
    // iterations) become the correct value.

    runner_running.store(true, std::memory_order_relaxed);
    while(should_run.load(std::memory_order_relaxed)) {
        auto iterationStartTime = current_time_ms();
        std::vector<int64_t> expiredTimes;
        std::vector<std::function<void()>> expiredTasks;

        if (idle) {
            idle = false;
            if (auto self = this->weak_from_this().lock()) {
                on_resume(self);
            }
        }

        // Accumulate any expired tasks
        {   
            SpinLockGuard guard(timerLock);
            for (auto& [timer_time, entries] : timers) {
                if (timer_time <= iterationStartTime) {
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

        // Drain a batch of tasks from the ready queue. We'll determine the batch
        // size with relaxed memory ordering because even a stale value is fine
        // since any stale values will be strictly less than the current value (this
        // method is the only method that _decrements_ the value and it does so inside
        // of a guard providing acquire/release fencing).
        std::vector<std::function<void()>> batch;
        auto queueSize = readyQueueSize.load(std::memory_order_relaxed);
        auto batchSize = std::min(queueSize, std::size_t(128));

        if (batchSize > 0) {
            SpinLockGuard guard(readyQueueLock);
            batch.reserve(batchSize);
            while(batch.size() < batchSize) {
                task = readyQueue.front();
                readyQueue.pop();
                batch.emplace_back(task);
            }

            // Decrement the ready queue size with a relaxed fence
            // since the SpinLockGuard above provides the necessary
            // release fence
            readyQueueSize.fetch_sub(batchSize, std::memory_order_relaxed);
        }

        // Execute the batch of tasks
        for(auto& task : batch) {
            task();
        }

        // Compute the next sleep time based on the current time, the next timer expiration
        // time, and the number of remaining tasks in the ready queue
        if (readyQueueSize.load(std::memory_order_relaxed) > 0) {
            sleep_time = std::chrono::milliseconds(0);
        } else {
            SpinLockGuard guard(timerLock);
            if (timers.empty()) {
                sleep_time = dormant_sleep_time;
            } else {
                auto next_timer = timers.begin();
                auto next_timer_time = next_timer->first;
                auto next_timer_sleep_time = std::chrono::milliseconds(next_timer_time - iterationStartTime);

                next_timer_sleep_time = std::min(next_timer_sleep_time, dormant_sleep_time);
                next_timer_sleep_time = std::max(next_timer_sleep_time, std::chrono::milliseconds(0));

                sleep_time = std::chrono::milliseconds(next_timer_sleep_time);
            }
        }

        // Idle the run thread only if we've detect that we should sleep for
        // some positive amount of time. This should be the _only_ time we
        // give the kernel the opportunity to sleep our run thread.
        //
        // We'll also call the on_idle callback if we're going to sleep. That
        // gives an opportunity for a higher-level scheduler to add work
        // if needed.
        if (sleep_time > std::chrono::milliseconds(0)) {
            
            idle = true;
            
            // Try and call the on_idle callback
            if (auto self = this->weak_from_this().lock()) {
                on_idle(self);
            }

            // Check again to see if any new tasks were stolen
            if (readyQueueSize.load(std::memory_order_relaxed) > 0) {
                continue;
            } else {
                // The idle callback produced no new work, so we can sleep
                std::unique_lock<std::mutex> lock(idlingThreadMutex);
                idlingThread.wait_for(lock, sleep_time);
            }
        }
    }

    // A relaxed fence is sufficient here since this is only used for
    // synchronization with the destructor and it's fine if the value
    // takes some amount of time to propagate.
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
        SpinLockGuard parent_guard(parent->timerLock);
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
        SpinLockGuard parent_guard(parent->timerLock);
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