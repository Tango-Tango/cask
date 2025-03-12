//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/scheduler/WorkStealingScheduler.hpp"
#include "cask/Config.hpp"
#include <cstdlib>
#include <cassert>
#include <iostream>

namespace cask::scheduler {

WorkStealingScheduler::WorkStealingScheduler(
    unsigned int poolSize,
    std::optional<int> priority)
    : data(std::make_shared<SchedulerData>())
{
    assert(poolSize > 1 && "Pool size must be greater than 1");
    auto idle_callback = std::bind(&WorkStealingScheduler::on_thread_idle, data);
    auto resume_callback = std::bind(&WorkStealingScheduler::on_thread_resume, data);
    auto request_work_callback = std::bind(&WorkStealingScheduler::on_thread_request_work, data, std::placeholders::_1);
    auto work_overflow_callback = std::bind(&WorkStealingScheduler::on_work_overflow, data, std::placeholders::_1);

    for(unsigned int i = 0; i < poolSize; i++) {
        auto sched = std::make_shared<SingleThreadScheduler>(
            priority,
            std::nullopt,
            config::work_steal_thread_queue_size,
            idle_callback,
            resume_callback,
            request_work_callback,
            work_overflow_callback,
            cask::scheduler::DisableAutoStart);
        auto thread_id = sched->get_run_thread_id();

        data->fixed.thread_ids.push_back(thread_id);
        data->fixed.schedulers.push_back(sched);
        data->fixed.schedulers_by_thread_id[thread_id] = sched;
        data->fixed.thread_id_indexes[thread_id] = i;
    }

    for(auto& sched : data->fixed.schedulers) {
        sched->start();
    }
}

WorkStealingScheduler::~WorkStealingScheduler() {
    // Signal all schedulers to stop
    for(auto& sched : data->fixed.schedulers) {
        sched->stop();
    }

    // Clear our reference to scheduler data
    data = nullptr;
}

bool WorkStealingScheduler::submit(const std::function<void()>& task) {
    auto thread_handle = std::this_thread::get_id();
    auto scheduler_lookup = data->fixed.schedulers_by_thread_id.find(thread_handle);

    if (scheduler_lookup != data->fixed.schedulers_by_thread_id.end()) {
        if (scheduler_lookup->second->submit(task)) {
            return true;
        }
    } else {
        auto dice_roll = static_cast<std::size_t>(std::abs(std::rand()));
        auto start_index = dice_roll % (data->fixed.schedulers.size());
        for(std::size_t i = 0; i < data->fixed.schedulers.size(); i++) {
            auto scheduler_index = (i + start_index) % data->fixed.schedulers.size();
            auto selected_scheduler = data->fixed.schedulers[scheduler_index];

            if (selected_scheduler->get_run_thread_id() != thread_handle && selected_scheduler->submit(task)) {
                return true;
            }
        }
    }

    // Fallback to submitting to the global ready queue
    {
        std::lock_guard<std::mutex> lock(data->synchronized.mutex);
        data->synchronized.global_ready_queue.emplace_back(task);

        // Try and wake up an idle scheduler - starting from
        // a random start point
        auto dice_roll = static_cast<std::size_t>(std::abs(std::rand()));
        auto scheduler_index = dice_roll % (data->fixed.schedulers.size());
        for(std::size_t i = 0; i < data->fixed.schedulers.size(); i++) {
            auto selected_scheduler = data->fixed.schedulers[(scheduler_index + i) % data->fixed.schedulers.size()];

            if (!selected_scheduler->isIdle()) continue;
            if (selected_scheduler->get_run_thread_id() == thread_handle) continue;

            selected_scheduler->try_wake();
            break;
        }

    }

    return true;
}

bool WorkStealingScheduler::submitBulk(const std::vector<std::function<void()>>& tasks) {
    auto thread_handle = std::this_thread::get_id();
    auto scheduler_lookup = data->fixed.schedulers_by_thread_id.find(thread_handle);

    if (scheduler_lookup != data->fixed.schedulers_by_thread_id.end()) {
        if (scheduler_lookup->second->submitBulk(tasks)) {
            return true;
        }
    } else {
        auto dice_roll = static_cast<std::size_t>(std::abs(std::rand()));
        auto start_index = dice_roll % (data->fixed.schedulers.size());
        for(std::size_t i = 0; i < data->fixed.schedulers.size(); i++) {
            auto scheduler_index = (i + start_index) % data->fixed.schedulers.size();
            auto selected_scheduler = data->fixed.schedulers[scheduler_index];

            if (selected_scheduler->get_run_thread_id() != thread_handle && selected_scheduler->submitBulk(tasks)) {
                return true;
            }
        }
    }

    // Fallback to submitting to the global ready queue
    {
        std::lock_guard<std::mutex> lock(data->synchronized.mutex);
        for(auto& task : tasks) {
            data->synchronized.global_ready_queue.emplace_back(task);
        }
    }

    return true;
}

CancelableRef WorkStealingScheduler::submitAfter(int64_t milliseconds, const std::function<void()>& task) {
    auto thread_handle = std::this_thread::get_id();
    auto scheduler_lookup = data->fixed.schedulers_by_thread_id.find(thread_handle);

    if (scheduler_lookup != data->fixed.schedulers_by_thread_id.end()) {
        return scheduler_lookup->second->submitAfter(milliseconds, task);
    } else {
        // Restrict candidates to idle schedulers
        std::vector<std::shared_ptr<SingleThreadScheduler>> candidates;
        for (auto& scheduler : data->fixed.schedulers) {
            if (scheduler->isIdle()) {
                candidates.emplace_back(scheduler);
            }
        }
        
        if (candidates.empty()) {
            // No idle schedulers, then just submit to any scheduler
            auto random_index = std::rand() % data->fixed.schedulers.size();
            return data->fixed.schedulers[random_index]->submitAfter(milliseconds, task);
        } else {
            // Submit to a random idle scheduler
            auto random_index = std::rand() % candidates.size();
            return candidates[random_index]->submitAfter(milliseconds, task);
        }
    }
}

bool WorkStealingScheduler::isIdle() const {
    return data->running_thread_count.load(std::memory_order_relaxed) == 0;
}

std::string WorkStealingScheduler::toString() const {
    return "WorkStealingScheduler_" + std::to_string(data->fixed.schedulers.size());
}

void WorkStealingScheduler::on_thread_idle(const std::weak_ptr<SchedulerData>& data_weak) {
    if (auto data = data_weak.lock()) {
        data->running_thread_count.fetch_sub(1, std::memory_order_relaxed);
    }
}

void WorkStealingScheduler::on_thread_resume(const std::weak_ptr<SchedulerData>& data_weak) {
    if (auto data = data_weak.lock()) {
        data->running_thread_count.fetch_add(1, std::memory_order_relaxed);
    }
}

void WorkStealingScheduler::on_thread_request_work(const std::weak_ptr<SchedulerData>& data_weak, ReadyQueue& requestor_ready_queue) {
    if (auto data = data_weak.lock()) {
        // Randomly prioritize taking from the global queue.
        // NOTE: Modulo by a prime helps ensure a more standard distribution
        auto prioritize_global_queue = (std::rand() % 61) == 0; // ~1.6% chance
        if (prioritize_global_queue) {
            if (drain_global_queue(data, requestor_ready_queue)) {
                return;
            }
        }

        if (steal_random_scheduler_unsafe(data, requestor_ready_queue)) {
            return;
        }

        // Fallback to taking work from the global ready queue
        if (!prioritize_global_queue) {
            drain_global_queue(data, requestor_ready_queue);
        }
    }
}


void WorkStealingScheduler::on_work_overflow(const std::weak_ptr<SchedulerData>& data_weak, std::deque<std::function<void()>>& overflowed_tasks) {
    if (auto data = data_weak.lock()) {
        std::lock_guard<std::mutex> lock(data->synchronized.mutex);
        for(auto& task : overflowed_tasks) {
            data->synchronized.global_ready_queue.emplace_back(task);
        }
    }
}

bool WorkStealingScheduler::drain_global_queue(const std::shared_ptr<SchedulerData>& data, ReadyQueue& requestor_ready_queue) {
    std::lock_guard<std::mutex> lock(data->synchronized.mutex);
    auto steal_size = std::min(config::work_steal_thread_queue_size / 2, data->synchronized.global_ready_queue.size());
    auto will_steal = steal_size > 0;

    while(steal_size != 0) {
        if (!requestor_ready_queue.push_back(data->synchronized.global_ready_queue.front())) {
            break;
        }
        data->synchronized.global_ready_queue.pop_front();
        steal_size--;
    }

    return will_steal;
}


bool WorkStealingScheduler::steal_random_scheduler_unsafe(const std::shared_ptr<SchedulerData>& data, ReadyQueue& requestor_ready_queue) {
    // Iterate through scheduler - starting from a random start point
    // trying to steal some work

    auto thread_handle = std::this_thread::get_id();
    auto dice_roll = static_cast<std::size_t>(std::abs(std::rand()));
    auto start_index = dice_roll % (data->fixed.schedulers.size());

    for(std::size_t i = 0; i < data->fixed.schedulers.size(); i++) {
        auto scheduler_index = (i + start_index) % data->fixed.schedulers.size();
        auto selected_scheduler = data->fixed.schedulers[scheduler_index];

        if (selected_scheduler->get_run_thread_id() != thread_handle && selected_scheduler->steal(requestor_ready_queue)) {
            return true;
        }
    }

    return false;
}

WorkStealingScheduler::FixedData::FixedData()
    : schedulers()
    , thread_ids()
    , thread_id_indexes()
    , schedulers_by_thread_id()
{}

WorkStealingScheduler::SychronizedData::SychronizedData()
    : mutex()
    , global_ready_queue()
{}

WorkStealingScheduler::SchedulerData::SchedulerData()
    : running_thread_count(0)
    , fixed()
    , synchronized()
{}

} // namespace cask::scheduler