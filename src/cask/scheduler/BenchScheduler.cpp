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
            ready_queue.emplace(std::get<1>(entry));
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

void BenchScheduler::submitAfter(int64_t milliseconds, const std::function<void()>& task) {
    std::lock_guard<std::mutex> guard(scheduler_mutex);
    int64_t scheduled_time = current_time + milliseconds;
    timers.emplace_back(scheduled_time, task);
}

bool BenchScheduler::isIdle() const {
    std::lock_guard<std::mutex> guard(scheduler_mutex);
    return timers.empty() && ready_queue.empty();
}

} // namespace cask::scheduler
