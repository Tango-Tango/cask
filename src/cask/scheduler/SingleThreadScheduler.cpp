//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/scheduler/SingleThreadScheduler.hpp"
#include "cask/Config.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stack>

#if __linux__
#include <sys/resource.h>
#elif __APPLE__
#include <pthread.h>
#include <sys/resource.h>
#endif

using namespace std::chrono_literals;

namespace cask::scheduler {


// NOLINTBEGIN(bugprone-easily-swappable-parameters)

SingleThreadScheduler::SingleThreadScheduler(
    std::optional<int> priority,
    std::optional<int> pinned_core,
    std::optional<std::size_t> max_queue_size,
    const std::function<void()>& on_idle,
    const std::function<void()>& on_resume,
    const std::function<void(ReadyQueue&)>& on_request_work,
    const std::function<void(std::deque<std::function<void()>>&)>& on_work_overflow,
    AutoStart auto_start)
    : priority(priority)
    , pinned_core(pinned_core)
    , max_queue_size(max_queue_size.value_or(std::numeric_limits<std::size_t>::max()))
    , started(false)
    , next_id(0)
    , run_thread_id()
    , control_data(std::make_shared<SchedulerControlData>(on_idle, on_resume, on_request_work, on_work_overflow))
{
    // Spawn the run thread
    std::thread run_thread(std::bind(&SingleThreadScheduler::run, control_data));

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

    if (auto_start == EnableAutoStart) {
        start();
    }
}

// NOLINTEND(bugprone-easily-swappable-parameters)

SingleThreadScheduler::~SingleThreadScheduler() {
    stop();
}

void SingleThreadScheduler::start() {
    control_data->start_barrier.notify();
    while(!control_data->thread_running.load(std::memory_order_acquire));
}

void SingleThreadScheduler::try_wake() {
    control_data->ready_queue.wake();
}

void SingleThreadScheduler::stop() {
    // Indicate that the run thread should shut down
    control_data->should_run = false;
    control_data->ready_queue.wake();

    // Wait for the run thread to finish - unless the destructor
    // is being called from the run thread itself.
    if (std::this_thread::get_id() != run_thread_id) {
        while(control_data->thread_running.load(std::memory_order_acquire));
    }
}

std::thread::id SingleThreadScheduler::get_run_thread_id() const {
    return run_thread_id;
}

bool SingleThreadScheduler::steal(ReadyQueue& requestor_ready_queue) {
    auto steal_happened = false;
    auto steal_size = control_data->ready_queue.size() / 2;

    while (steal_size > 0) {
        if (requestor_ready_queue.steal_from(control_data->ready_queue)) {
            steal_happened = true;
            steal_size--;
        } else {
            break;
        }
    }

    return steal_happened;
}

bool SingleThreadScheduler::submit(const std::function<void()>& task) {
    return control_data->ready_queue.push_back(task);
}

bool SingleThreadScheduler::submitBulk(const std::vector<std::function<void()>>& tasks) {
    return control_data->ready_queue.push_batch_back(tasks);
}

CancelableRef SingleThreadScheduler::submitAfter(int64_t milliseconds, const std::function<void()>& task) {
    std::lock_guard<std::mutex> lock(control_data->timers_mutex);

    TimerId id = next_id++;
    TimerTimeMs executionTick = current_time_ms() + milliseconds;

    auto timer = std::make_shared<CancelableTimer>(
        control_data,
        executionTick,
        id
    );

    timer->onShutdown(task);

    auto timers = control_data->timers.find(executionTick);
    
    if(timers == control_data->timers.end()) {
        std::vector<std::shared_ptr<CancelableTimer>> taskVector = {timer};
        control_data->timers[executionTick] = taskVector;
    } else {
        timers->second.push_back(timer);
    }

    return timer;
}

bool SingleThreadScheduler::isIdle() const {
    return control_data->idle.load(std::memory_order_relaxed);
} 

std::string SingleThreadScheduler::toString() const {
    return "SingleThreadScheduler";
}

void SingleThreadScheduler::run(const std::shared_ptr<SchedulerControlData>& control_data) {
    std::optional<std::function<void()>> task = std::nullopt;

    // Wait for the signal that the scheduler is ready to run
    control_data->start_barrier.wait();

    // We've been told to start running
    control_data->thread_running.store(true, std::memory_order_release);

    while(true) {
        // Check if we should be shutting down
        if (!control_data->should_run) break;

        // Evaluate any expired timers
        {
            std::lock_guard<std::mutex> timers_lock(control_data->timers_mutex);
            auto overflow = evaluate_timers_unsafe(control_data);

            // Push any overflowed tasks to the parent scheduler
            if (!overflow.empty()) {
                control_data->on_work_overflow(overflow);
            }
        }

        // Take an item from the ready queue
        task = control_data->ready_queue.pop_front();

        // If we are transitioning from idle to running, call the on_resume callback
        if (control_data->idle && task.has_value()) {
            control_data->idle.store(false, std::memory_order_relaxed);
            control_data->on_resume();
        }

        // If we have work to do, then release the lock and do it
        if (task.has_value()) {
            (*task)();
            task.reset();
        }

        // After an execution attempt, check if we should be idle
        if (control_data->ready_queue.empty()) {
            // Our ready queue is empty. Attempt to fill it by stealing
            // work from another scheduler.
            control_data->on_request_work(control_data->ready_queue);

            auto next_sleep_time = std::chrono::milliseconds(0);

            if (control_data->ready_queue.empty()) {
                // Didn't find any work to steal. Look at timers and figure
                // out a suitable time to sleep for.
                std::lock_guard<std::mutex> timers_lock(control_data->timers_mutex);

                // On-average, set a minimum sleep time of 10ms. We'll allow
                // some deviation from this average (+- 2ms) so that threads
                // spread out in time when they wake.
                next_sleep_time = std::chrono::milliseconds(8);
                next_sleep_time += std::chrono::milliseconds(std::rand() % 5);

                // Compute the next timer sleep time (if any)
                auto next_timer = control_data->timers.begin();
                if (next_timer != control_data->timers.end()) {
                    auto current_time = std::chrono::milliseconds(current_time_ms());
                    auto next_timer_time = std::chrono::milliseconds(next_timer->first);

                    if (current_time >= next_timer_time) {
                        next_sleep_time = std::chrono::milliseconds::zero();
                    } else {
                        next_sleep_time = std::min(next_sleep_time, next_timer_time - current_time);
                    }
                }
            }

            // Look at the chosen sleep time and see if we need to go to sleep
            if (next_sleep_time > std::chrono::milliseconds::zero()) {
                // If we are transitioning to idle call the on_idle callback
                if (!control_data->idle) {
                    control_data->idle.store(true, std::memory_order_relaxed);
                    control_data->on_idle();
                }

                // Nap time!
                control_data->ready_queue.await_work(next_sleep_time);
            }   
        }
    }

    // Indicate the run thread has shut down.
    control_data->thread_running.store(false, std::memory_order_release);
}

std::deque<std::function<void()>> SingleThreadScheduler::evaluate_timers_unsafe(const std::shared_ptr<SchedulerControlData>& control_data) {
    auto iterationStartTime = current_time_ms();
    std::vector<int64_t> expiredTimes;
    std::deque<std::function<void()>> overflowed_tasks;

    // Accumulate any expired tasks
    for (auto& [timer_time, timers] : control_data->timers) {
        if (timer_time <= iterationStartTime) {
            for(auto& timer : timers) {
                auto task = [timer] { timer->fire(); };

                if (auto overflow = control_data->ready_queue.push_front(task)) {
                    overflowed_tasks.push_front(*overflow);
                }
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

    return overflowed_tasks;
}

bool SingleThreadScheduler::steal_unsafe(const std::shared_ptr<SchedulerControlData>& control_data, std::deque<std::function<void()>>& requestor_ready_queue, std::size_t requested_amount) {
    std::function<void()> task;
    bool work_stolen = false;

    while(!control_data->ready_queue.empty() && requested_amount != 0) {
        requested_amount--;

        if (auto task = control_data->ready_queue.pop_front()) {
            requestor_ready_queue.emplace_back(*task);
            work_stolen = true;
        } else {
            break;
        }
    }

    return work_stolen;
}

int64_t SingleThreadScheduler::current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)

SingleThreadScheduler::SchedulerControlData::SchedulerControlData(
    const std::function<void()>& on_idle,
    const std::function<void()>& on_resume,
    const std::function<void(ReadyQueue&)>& on_request_work,
    const std::function<void(std::deque<std::function<void()>>&)>& on_work_overflow
)   : thread_running(false)
    , start_barrier()
    , should_run(true)
    , idle(true)
    , ready_queue()
    , timers_mutex()
    , timers()
    , on_idle(on_idle)
    , on_resume(on_resume)
    , on_request_work(on_request_work)
    , on_work_overflow(on_work_overflow)
{}


SingleThreadScheduler::CancelableTimer::CancelableTimer(
    const std::shared_ptr<SchedulerControlData>& control_data,
    int64_t time_slot,
    int64_t id
)   : control_data(control_data)
    , time_slot(time_slot)
    , id(id)
    , shutdown_callbacks()
    , cancel_callbacks()
    , timer_mutex()
    , canceled(false)
    , shutdown(false)
{}

// NOLINTEND(bugprone-easily-swappable-parameters)

void SingleThreadScheduler::CancelableTimer::fire() {
    std::vector<std::function<void()>> callbacks_to_run;

    {
        std::lock_guard<std::mutex> self_guard(timer_mutex);
        if (!shutdown && !canceled) {
            shutdown = true;
            std::swap(shutdown_callbacks, callbacks_to_run);
        }
    }

    for(auto& cb : callbacks_to_run) {
        cb();
    }
}

void SingleThreadScheduler::CancelableTimer::cancel() {
    std::vector<std::function<void()>> callbacks_to_run;

    {
        std::lock_guard<std::mutex> parent_guard(control_data->timers_mutex);
        std::lock_guard<std::mutex> self_guard(timer_mutex);

        if (!shutdown && !canceled) {
            auto tasks = control_data->timers.find(time_slot);
            if(tasks != control_data->timers.end()) {
                std::vector<std::shared_ptr<CancelableTimer>> filteredTimers;
                auto timers = tasks->second;

                for(auto& timer : timers) {
                    if(timer->id != id) {
                        filteredTimers.emplace_back(timer);
                    } else {
                        canceled = true;
                    }
                }

                if(filteredTimers.size() > 0) {
                    control_data->timers[time_slot] = filteredTimers;
                } else {
                    control_data->timers.erase(time_slot);
                }

                if (canceled) {
                    std::swap(cancel_callbacks, callbacks_to_run);
                }
            }
        }
    }

    for(auto& cb : callbacks_to_run) {
        cb();
    }
}

void SingleThreadScheduler::CancelableTimer::onCancel(const std::function<void()>& callback) {
    bool run_callback_now = false;

    {
        std::lock_guard<std::mutex> guard(timer_mutex);

        if(canceled) {
            run_callback_now = true;
        } else {
            cancel_callbacks.emplace_back(callback);
        }
    }

    if (run_callback_now) {
        callback();
    }
}

void SingleThreadScheduler::CancelableTimer::onShutdown(const std::function<void()>& callback) {
    bool run_callback_now = false;

    {
        std::lock_guard<std::mutex> guard(timer_mutex);

        if(shutdown) {
            run_callback_now = true;
        } else {
            shutdown_callbacks.emplace_back(callback);
        }
    }

    if (run_callback_now) {
        callback();
    }
}

} // namespace cask::scheduler
