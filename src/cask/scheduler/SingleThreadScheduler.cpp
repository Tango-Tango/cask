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
    std::optional<std::size_t> batch_size,
    const std::function<void()>& on_idle,
    const std::function<void()>& on_resume,
    const std::function<std::vector<std::function<void()>>(std::size_t requested_amount)>& on_request_work)
    : control_data(std::make_shared<SchedulerControlData>(on_idle, on_resume, on_request_work))
{
    auto actual_batch_size = batch_size.value_or(DEFAULT_BATCH_SIZE);

    // Spawn the run thread
    std::thread run_thread(std::bind(&SingleThreadScheduler::run, actual_batch_size, control_data));

    // Set the thread priority if requested
    if (priority.has_value()) {
#if __linux__
        auto which = PRIO_PROCESS;
        setpriority(which, run_thread.native_handle(), *priority);
#elif __APPLE__
        auto which = PRIO_PROCESS;
        uint64_t run_thread_id = 0;

        pthread_threadid_np(run_thread.native_handle(), &run_thread_id);

        setpriority(which, run_thread_id, *priority);
#else
        std::cerr << "Setting thread priority is not supported on this platform." << std::endl;
#endif
    }

    // Set the thread affinity if requested
    if (pinned_core.has_value()) {
#if defined(__linux__) && defined(_GNU_SOURCE)
        auto handle = run_thread.native_handle();
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(*pinned_core, &cpuset);
        pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset);
#else
        std::cerr << "Setting thread affinity is not supported on this platform." << std::endl;
#endif
    }

    // Detach the run thread and ensure it is running before returning
    run_thread_id = run_thread.get_id();
    run_thread.detach();
    while(!control_data->thread_running.load(std::memory_order_acquire));
}

SingleThreadScheduler::~SingleThreadScheduler() {
    // Incidate that the run thread should shut down
    {
        std::lock_guard<std::mutex> lock(control_data->mutex);
        control_data->should_run = false;
        control_data->work_available.notify_all();
    }

    // Wait for the run thread to finish - unless the destructor
    // is being called from the run thread itself.
    if (std::this_thread::get_id() != run_thread_id) {
        while(control_data->thread_running.load(std::memory_order_acquire));
    }
}

std::thread::id SingleThreadScheduler::get_run_thread_id() const {
    return run_thread_id;
}

std::vector<std::function<void()>> SingleThreadScheduler::steal(std::size_t batch_size) {
    std::lock_guard<std::mutex> lock(control_data->mutex);

    std::function<void()> task;
    std::vector<std::function<void()>> batch;

    auto batchSize = std::min(control_data->ready_queue.size(), batch_size);
    batch.reserve(batchSize);

    while(batch.size() < batchSize) {
        task = control_data->ready_queue.front();
        control_data->ready_queue.pop();
        batch.emplace_back(task);
    }

    return batch;
}

void SingleThreadScheduler::submit(const std::function<void()>& task) {
    std::lock_guard<std::mutex> lock(control_data->mutex);
    control_data->ready_queue.emplace(task);
    control_data->work_available.notify_all();
}

void SingleThreadScheduler::submitBulk(const std::vector<std::function<void()>>& tasks) {
    std::lock_guard<std::mutex> lock(control_data->mutex);
    
    for(auto& task: tasks) {
        control_data->ready_queue.emplace(task);
    }

    control_data->work_available.notify_all();
}

CancelableRef SingleThreadScheduler::submitAfter(int64_t milliseconds, const std::function<void()>& task) {
    std::lock_guard<std::mutex> lock(control_data->mutex);

    auto id = next_id++;
    auto executionTick = current_time_ms() + milliseconds;

    auto tasks = control_data->timers.find(executionTick);
    auto entry = std::make_tuple(id, task);
    
    if(tasks == control_data->timers.end()) {
        std::vector<TimerEntry> taskVector = {entry};
        control_data->timers[executionTick] = taskVector;
    } else {
        tasks->second.push_back(entry);
    }

    auto cancelable = std::make_shared<CancelableTimer>(
        this->control_data,
        executionTick,
        id
    );

    control_data->work_available.notify_all();

    return cancelable;
}

bool SingleThreadScheduler::isIdle() const {
    std::lock_guard<std::mutex> lock(control_data->mutex);
    return control_data->idle;
}

std::string SingleThreadScheduler::toString() const {
    return "SingleThreadScheduler";
}

void SingleThreadScheduler::run(const std::size_t batch_size, const std::shared_ptr<SchedulerControlData>& control_data) {
    std::function<void()> task;

    // Indicate the run thread is running
    control_data->thread_running.store(true, std::memory_order_release);

    while(true) {
        std::vector<std::function<void()>> batch;
        auto iterationStartTime = current_time_ms();

        // Grab the lock and accumulate a batch of work including any
        // expired timers, tasks drained from the ready queue, and
        // finally work stolen from other schedulers.
        {
            std::lock_guard<std::mutex> lock(control_data->mutex);

            // While hold the lock check if we should even be
            // running at all.
            if (!control_data->should_run) break;

            std::vector<int64_t> expiredTimes;

            // Accumulate any expired tasks
            for (auto& [timer_time, entries] : control_data->timers) {
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
                control_data->timers.erase(timerTime);
            }

            // Fill the batch with ready tasks by draining the ready queue
            auto batchSize = std::min(control_data->ready_queue.size(), std::size_t(batch_size));
            while(batch.size() < batchSize) {
                task = control_data->ready_queue.front();
                control_data->ready_queue.pop();
                batch.emplace_back(task);
            }

            // If we didn't find any work, request some from the parent scheduler
            if (batch.empty()) {
                batch = control_data->on_request_work(batch_size);
            }

            // If we are transitioning from idle to running, call the on_resume callback
            if (control_data->idle && !batch.empty()) {
                control_data->idle = false;
                control_data->on_resume();
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
            std::unique_lock<std::mutex> lock(control_data->mutex);

            // Once again check if we should even be running at all while
            // holding the lock.
            if (!control_data->should_run) break;

            if (control_data->ready_queue.empty()) {
                // If we have no work to do, sleep until either the next timer is ready or
                // some random amount of time to wake up and look for work. This will avoid
                // all of the schedulers looking for work "at the same time" when operating
                // in a work-starved environment.

                // Compute a random sleep time between 0 and 100ms
                auto next_sleep_time = std::chrono::milliseconds(std::abs(std::rand()) % 100);

                // Compute the next timer sleep time
                auto next_timer = control_data->timers.begin();
                if (next_timer != control_data->timers.end()) {
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
                    if (!control_data->idle) {
                        control_data->idle = true;
                        control_data->on_idle();
                    }

                    // Nap time!
                    control_data->work_available.wait_for(lock, next_sleep_time);

                    // Once again we are now holding the lock. See if we were woken up
                    // because we should be shutting down
                    if (!control_data->should_run) break;
                }
            }
        }
    }

    // Indicate the run thread has shut down.
    control_data->thread_running.store(false, std::memory_order_release);
}

int64_t SingleThreadScheduler::current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

SingleThreadScheduler::SchedulerControlData::SchedulerControlData(
    const std::function<void()>& on_idle,
    const std::function<void()>& on_resume,
    const std::function<std::vector<std::function<void()>>(std::size_t)>& on_request_work
)   : thread_running(false)
    , mutex()
    , work_available()
    , should_run(true)
    , idle(true)
    , timers()
    , ready_queue()
    , on_idle(on_idle)
    , on_resume(on_resume)
    , on_request_work(on_request_work)
{}

SingleThreadScheduler::CancelableTimer::CancelableTimer(
    const std::shared_ptr<SchedulerControlData>& control_data,
    int64_t time_slot,
    int64_t id
)   : control_data(control_data)
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
        std::lock_guard<std::mutex> control_guard(control_data->mutex);
        std::lock_guard<std::mutex> self_guard(callback_mutex);
        auto tasks = control_data->timers.find(time_slot);
        if(tasks != control_data->timers.end()) {
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
                control_data->timers[time_slot] = filteredEntries;
            } else {
                control_data->timers.erase(time_slot);
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

    std::lock_guard<std::mutex> control_guard(control_data->mutex);
    std::lock_guard<std::mutex> self_guard(callback_mutex);
    auto tasks = control_data->timers.find(time_slot);

    if(tasks != control_data->timers.end()) {
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

        control_data->timers[time_slot] = newEntries;
    }

    if(!found) {
        shutdownCallback();
    }
}

} // namespace cask::scheduler
