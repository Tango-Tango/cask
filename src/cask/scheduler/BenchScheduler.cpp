//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/scheduler/BenchScheduler.hpp"

namespace cask::scheduler {

BenchScheduler::BenchScheduler()
    : current_time(0)
    , ready_queue()
    , timers()
    , scheduler_mutex()
{}

std::size_t BenchScheduler::num_task_ready() const {
    std::lock_guard<std::mutex> guard(scheduler_mutex);
    return ready_queue.size();
}

std::size_t BenchScheduler::num_timers() const {
    std::lock_guard<std::mutex> guard(scheduler_mutex);
    return timers.size();
}

bool BenchScheduler::run_one_task() {
    std::function<void()> task;

    {
        std::lock_guard<std::mutex> guard(scheduler_mutex);
        if(!ready_queue.empty()) {
            task = ready_queue.front();
            ready_queue.pop();
        }
    }

    if(task) {
        task();
        return true;
    } else {
        return false;
    }
}

std::size_t BenchScheduler::run_ready_tasks() {
    std::size_t num_executed = 0;
    while(run_one_task()) num_executed++;
    return num_executed;
}

void BenchScheduler::advance_time(int64_t milliseconds) {
    std::lock_guard<std::mutex> guard(scheduler_mutex);
    current_time += milliseconds;
    
    std::vector<TimerEntry> new_timers;
    for(auto& entry : timers) {
        if(std::get<0>(entry) <= current_time) {
            ready_queue.emplace(std::get<2>(entry));
        } else {
            new_timers.emplace_back(entry);
        }
    }

    timers = new_timers;
}

void BenchScheduler::submit(const std::function<void()>& task) {
    std::lock_guard<std::mutex> guard(scheduler_mutex);
    ready_queue.emplace(task);
}

void BenchScheduler::submitBulk(const std::vector<std::function<void()>>& tasks) {
    std::lock_guard<std::mutex> guard(scheduler_mutex);
    for(auto& task : tasks) {
        ready_queue.emplace(task);
    }
}

CancelableRef BenchScheduler::submitAfter(int64_t milliseconds, const std::function<void()>& task) {
    std::lock_guard<std::mutex> guard(scheduler_mutex);
    int64_t scheduled_time = current_time + milliseconds;
    int64_t id = next_id++;

    auto cancelable = std::make_shared<BenchCancelableTimer>(
        this->shared_from_this(),
        id
    );

    timers.emplace_back(scheduled_time, id, task);

    return cancelable;
}

bool BenchScheduler::isIdle() const {
    std::lock_guard<std::mutex> guard(scheduler_mutex);
    return timers.empty() && ready_queue.empty();
}

BenchScheduler::BenchCancelableTimer::BenchCancelableTimer(
    const std::shared_ptr<BenchScheduler>& parent,
    int64_t id
)   : parent(parent)
    , id(id)
    , callbacks()
    , callback_mutex()
    , canceled(false)
{}

void BenchScheduler::BenchCancelableTimer::cancel() {
    if(canceled) {
        return;
    } else {
        std::lock_guard<std::mutex> self_guard(callback_mutex);
        std::lock_guard<std::mutex> parent_guard(parent->scheduler_mutex);
        std::vector<TimerEntry> filteredEntries;

        for(auto& entry : parent->timers) {
            auto entry_id = std::get<1>(entry);
            if(entry_id != id) {
                filteredEntries.emplace_back(entry);
            } else {
                canceled = true;
            }
        }

        parent->timers = filteredEntries;
    }

    if(canceled) {
        for(auto& cb : callbacks) {
            cb();
        }
    }
}

void BenchScheduler::BenchCancelableTimer::onCancel(const std::function<void()>& callback) {
    if(canceled) {
        callback();
    } else {
        std::lock_guard<std::mutex> guard(callback_mutex);
        callbacks.emplace_back(callback);
    }
}

void BenchScheduler::BenchCancelableTimer::onShutdown(const std::function<void()>&) {
    // Not implemented for the bench scheduler
}

} // namespace cask::scheduler
