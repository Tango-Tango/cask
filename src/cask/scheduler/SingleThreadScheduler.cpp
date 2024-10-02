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
    std::function<void()> on_idle,
    std::function<void()> on_resume,
    std::function<std::vector<std::function<void()>>()> on_request_work)
    : on_idle(on_idle)
    , on_resume(on_resume)
    , on_request_work(on_request_work)
    , should_run(true)
    , idle(true)
    , runner_running(false)
    , readyQueue()
    , timers()
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
    workAvailable.notify_all();

    // Wait for the run thread to stop. Use a relaxed fence since we're
    // just waiting for the thread to stop and it's fine if the value
    // takes some time to propagate.
    while(runner_running.load(std::memory_order_relaxed));
}

std::thread::id SingleThreadScheduler::run_thread_id() const {
    return runThreadId;
}

std::vector<std::function<void()>> SingleThreadScheduler::steal(std::size_t batch_size) {
    std::lock_guard<std::mutex> lock(mutex);

    std::function<void()> task;
    std::vector<std::function<void()>> batch;

    auto batchSize = std::min(readyQueue.size(), batch_size);
    batch.reserve(batchSize);

    while(batch.size() < batchSize) {
        task = readyQueue.front();
        readyQueue.pop();
        batch.emplace_back(task);
    }

    return batch;
}

void SingleThreadScheduler::submit(const std::function<void()>& task) {
    std::lock_guard<std::mutex> lock(mutex);
    readyQueue.emplace(task);
    workAvailable.notify_all();
}

void SingleThreadScheduler::submitBulk(const std::vector<std::function<void()>>& tasks) {
    std::lock_guard<std::mutex> lock(mutex);
    
    for(auto& task: tasks) {
        readyQueue.emplace(task);
    }

    workAvailable.notify_all();
}

CancelableRef SingleThreadScheduler::submitAfter(int64_t milliseconds, const std::function<void()>& task) {
    std::lock_guard<std::mutex> lock(mutex);

    auto id = next_id++;
    auto executionTick = current_time_ms() + milliseconds;

    auto tasks = timers.find(executionTick);
    auto entry = std::make_tuple(id, task);
    
    if(tasks == timers.end()) {
        std::vector<TimerEntry> taskVector = {entry};
        timers[executionTick] = taskVector;
    } else {
        tasks->second.push_back(entry);
    }

    auto cancelable = std::make_shared<CancelableTimer>(
        this->shared_from_this(),
        executionTick,
        id
    );

    workAvailable.notify_all();

    return cancelable;
}

bool SingleThreadScheduler::isIdle() const {
    std::lock_guard<std::mutex> lock(mutex);
    return idle;
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
        std::vector<std::function<void()>> batch;
        auto iterationStartTime = current_time_ms();

        // Grab the lock and accumulate a batch of work including any
        // expired timers, tasks drained from the ready queue, and
        // finally work stolen from other schedulers.
        {
            std::lock_guard<std::mutex> lock(mutex);
            std::vector<int64_t> expiredTimes;

            // Accumulate any expired tasks
            for (auto& [timer_time, entries] : timers) {
                if (timer_time <= iterationStartTime) {
                    for(auto& entry : entries) {
                        batch.emplace_back(std::get<1>(entry));
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

            // Fill the batch with ready tasks by draining the ready queue
            auto batchSize = std::min(readyQueue.size(), std::size_t(128));
            while(batch.size() < batchSize) {
                task = readyQueue.front();
                readyQueue.pop();
                batch.emplace_back(task);
            }

            // If we didn't find any work, request some from the parent scheduler
            if (batch.empty()) {
                batch = on_request_work();
            }

            // If we are transitioning from idle to running, call the on_resume callback
            if (idle && !batch.empty()) {
                idle = false;
                on_resume();
            }
        }

        // Execute the batch of tasks
        for(auto& task : batch) {
            task();
        }

        {
            std::unique_lock<std::mutex> lock(mutex);

            if (readyQueue.empty()) {
                auto next_timer = timers.begin();
                auto next_timer_time = next_timer->first;
                auto next_timer_sleep_time = std::chrono::milliseconds(next_timer_time - iterationStartTime);

                next_timer_sleep_time = std::min(next_timer_sleep_time, dormant_sleep_time);
                next_timer_sleep_time = std::max(next_timer_sleep_time, std::chrono::milliseconds(0));

                sleep_time = std::chrono::milliseconds(next_timer_sleep_time);

                if (!idle) {
                    idle = true;
                    on_idle();
                }

                workAvailable.wait_for(lock, sleep_time);
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
        std::lock_guard<std::mutex> parent_guard(parent->mutex);
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
        std::lock_guard<std::mutex> parent_guard(parent->mutex);
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