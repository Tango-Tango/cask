//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/scheduler/SingleThreadScheduler.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>

#if __linux__
#include <sys/resource.h>
#elif __APPLE__
#include <pthread.h>
#include <sys/resource.h>
#endif

using namespace std::chrono_literals;

namespace cask::scheduler {

SingleThreadScheduler::SingleThreadScheduler(
    std::optional<int> priority,
    std::optional<int> pinned_core,
    const std::function<void()>& on_idle,
    const std::function<void()>& on_resume,
    const std::function<std::vector<std::function<void()>>()>& on_request_work)
    : on_idle(on_idle)
    , on_resume(on_resume)
    , on_request_work(on_request_work)
    , should_run(true)
    , idle(true)
    , runner_running(false)
    , readyQueue()
    , timers()
{
    // Spawn the run thread
    std::thread runThread(std::bind(&SingleThreadScheduler::run, this));

    // Set the thread priority if requested
    if (priority.has_value()) {
#if __linux__
        auto which = PRIO_PROCESS;
        setpriority(which, runThread.native_handle(), *priority);
#elif __APPLE__
        auto which = PRIO_PROCESS;
        uint64_t run_thread_id = 0;

        pthread_threadid_np(runThread.native_handle(), &run_thread_id);

        setpriority(which, run_thread_id, *priority);
#else
        std::cerr << "Setting thread priority is not supported on this platform." << std::endl;
#endif
    }

    // Set the thread affinity if requested
    if (pinned_core.has_value()) {
#if defined(__linux__) && defined(_GNU_SOURCE)
        auto handle = runThread.native_handle();
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(*pinned_core, &cpuset);
        pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset);
#else
        std::cerr << "Setting thread affinity is not supported on this platform." << std::endl;
#endif
    }

    // Detach the run thread and ensure it is running before returning
    runThreadId = runThread.get_id();
    runThread.detach();
    while(!runner_running.load(std::memory_order_acquire));
}

SingleThreadScheduler::~SingleThreadScheduler() {
    // Incidate that the run thread should shut down
    {
        std::lock_guard<std::mutex> lock(mutex);
        should_run = false;
        workAvailable.notify_all();
    }

    // Wait for the run thread to finish
    while(runner_running.load(std::memory_order_acquire));
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
        this->weak_from_this(),
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

std::string SingleThreadScheduler::toString() const {
    return "SingleThreadScheduler";
}

void SingleThreadScheduler::run() {
    std::function<void()> task;

    // Indicate the run thread is running
    runner_running.store(true, std::memory_order_release);

    while(true) {
        std::vector<std::function<void()>> batch;
        auto iterationStartTime = current_time_ms();

        // Grab the lock and accumulate a batch of work including any
        // expired timers, tasks drained from the ready queue, and
        // finally work stolen from other schedulers.
        {
            std::lock_guard<std::mutex> lock(mutex);

            // While hold the lock check if we should even be
            // running at all.
            if (!should_run) break;

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

        // Execute the batch of tasks. This is done outside of the lock to avoid
        // deadlocks and contention both with the running tasks which may be
        // submitting work and with other schedulers that may attempt to steal
        // work while we are busy.
        for(auto& task : batch) {
            task();
        }

        {
            std::unique_lock<std::mutex> lock(mutex);

            // Once again check if we should even be running at all while
            // holding the lock.
            if (!should_run) break;

            if (readyQueue.empty()) {
                // If we have no work to do, sleep until either the next timer is ready or
                // some random amount of time to wake up and look for work. This will avoid
                // all of the schedulers looking for work "at the same time" when operating
                // in a work-starved environment.

                // Compute a random sleep time between 0 and 100ms
                auto next_sleep_time = std::chrono::milliseconds(std::abs(std::rand()) % 100);

                // Compute the next timer sleep time
                auto next_timer = timers.begin();
                if (next_timer != timers.end()) {
                    auto next_timer_time = next_timer->first;
                    auto next_timer_expires_in = std::chrono::milliseconds(next_timer_time - iterationStartTime);
                    next_sleep_time = std::min(next_sleep_time, next_timer_expires_in);
                    next_sleep_time = std::max(next_sleep_time, std::chrono::milliseconds(0));
                }

                // There is a possibility we've chosen to sleep for 0 milliseconds either randomly or because
                // a timer needs to fire immediately. In that case we won't transition to idle and instead
                // will immediately check for more work.

                if (next_sleep_time > std::chrono::milliseconds::zero()) {
                    // If we are transitioning to idle call the on_idle callback
                    if (!idle) {
                        idle = true;
                        on_idle();
                    }

                    // Nap time!
                    workAvailable.wait_for(lock, next_sleep_time);

                    // Once again we are now holding the lock. See if we were woken up
                    // because we should be shutting down
                    if (!should_run) break;
                }
            }
        }
    }

    // Indicate the run thread has shut down.
    runner_running.store(false, std::memory_order_release);
}

int64_t SingleThreadScheduler::current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

SingleThreadScheduler::CancelableTimer::CancelableTimer(
    const std::weak_ptr<SingleThreadScheduler>& parent_weak,
    int64_t time_slot,
    int64_t id
)   : parent_weak(parent_weak)
    , time_slot(time_slot)
    , id(id)
    , callbacks()
    , callback_mutex()
    , canceled(false)
{}

void SingleThreadScheduler::CancelableTimer::cancel() {
    if(canceled) {
        return;
    } else if(auto parent = parent_weak.lock()) {
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

    if (auto parent = parent_weak.lock()) {
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
