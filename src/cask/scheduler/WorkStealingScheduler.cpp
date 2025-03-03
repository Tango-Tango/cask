//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/scheduler/WorkStealingScheduler.hpp"
#include <cstdlib>
#include <cassert>

namespace cask::scheduler {

WorkStealingScheduler::WorkStealingScheduler(
    unsigned int poolSize,
    std::optional<int> priority,
    std::optional<std::size_t> batch_size)
    : running_thread_count(0)
{
    assert(poolSize > 1 && "Pool size must be greater than 1");
    auto idle_callback = std::bind(&WorkStealingScheduler::onThreadIdle, this->weak_from_this());
    auto resume_callback = std::bind(&WorkStealingScheduler::onThreadResume, this->weak_from_this());
    auto request_work_callback = std::bind(&WorkStealingScheduler::onThreadRequestWork, this->weak_from_this(), std::placeholders::_1);

    for(unsigned int i = 0; i < poolSize; i++) {
        auto sched = std::make_shared<SingleThreadScheduler>(priority, std::nullopt, batch_size, idle_callback, resume_callback, request_work_callback);
        auto thread_id = sched->get_run_thread_id();

        thread_ids.push_back(thread_id);
        schedulers[thread_id] = sched;
        thread_id_indexes[thread_id] = i;
    }
}

void WorkStealingScheduler::submit(const std::function<void()>& task) {
    auto thread_handle = std::this_thread::get_id();
    auto scheduler_lookup = schedulers.find(thread_handle);

    if (scheduler_lookup != schedulers.end()) {
        scheduler_lookup->second->submit(task);
    } else {
        auto random_index = std::rand() % thread_ids.size();
        auto random_thread_id = thread_ids[random_index];
        schedulers[random_thread_id]->submit(task);
    }
}

void WorkStealingScheduler::submitBulk(const std::vector<std::function<void()>>& tasks) {
    auto thread_handle = std::this_thread::get_id();
    auto scheduler_lookup = schedulers.find(thread_handle);

    if (scheduler_lookup != schedulers.end()) {
        scheduler_lookup->second->submitBulk(tasks);
    } else {
        auto random_index = std::rand() % thread_ids.size();
        auto random_thread_id = thread_ids[random_index];
        schedulers[random_thread_id]->submitBulk(tasks);
    }
}

CancelableRef WorkStealingScheduler::submitAfter(int64_t milliseconds, const std::function<void()>& task) {
    auto thread_handle = std::this_thread::get_id();
    auto scheduler_lookup = schedulers.find(thread_handle);

    if (scheduler_lookup != schedulers.end()) {
        return scheduler_lookup->second->submitAfter(milliseconds, task);
    } else {
        auto random_index = std::rand() % thread_ids.size();
        auto random_thread_id = thread_ids[random_index];
        return schedulers[random_thread_id]->submitAfter(milliseconds, task);
    }
}

bool WorkStealingScheduler::isIdle() const {
    return running_thread_count.load(std::memory_order_relaxed) == 0;
}

std::string WorkStealingScheduler::toString() const {
    return "WorkStealingScheduler_" + std::to_string(schedulers.size());
}

void WorkStealingScheduler::onThreadIdle(const std::weak_ptr<WorkStealingScheduler>& self_weak) {
    if (auto self = self_weak.lock()) {
        self->running_thread_count.fetch_sub(1, std::memory_order_relaxed);
    }
}

void WorkStealingScheduler::onThreadResume(const std::weak_ptr<WorkStealingScheduler>& self_weak) {
    if (auto self = self_weak.lock()) {
        self->running_thread_count.fetch_add(1, std::memory_order_relaxed);
    }
}

std::vector<std::function<void()>> WorkStealingScheduler::onThreadRequestWork(const std::weak_ptr<WorkStealingScheduler>& self_weak, std::size_t amount_requested) {
    auto current_thread_id = std::this_thread::get_id();

    if (auto self = self_weak.lock()) {

        // Attempt to lookup the current scheduler's index
        auto current_thread_index = self->thread_id_indexes.find(current_thread_id);
        if (current_thread_index == self->thread_id_indexes.end()) {
            return {};
        }

        // Select a random scheduler to steal from, adjusting so we exclude the current one
        auto dice_roll = static_cast<std::size_t>(std::abs(std::rand()));
        auto scheduler_index = dice_roll % (self->thread_ids.size() - 1);

        if (scheduler_index >= current_thread_index->second) {
            scheduler_index++;
        }

        auto thread_id = self->thread_ids[scheduler_index];

        // Check if the scheduler exists. If it doesn't then just give up since
        // we are probably shutting down.
        auto scheduler_lookup = self->schedulers.find(thread_id);
        if (scheduler_lookup == self->schedulers.end()) {
            return {};
        }

        // Try and steal some work
        return scheduler_lookup->second->steal(amount_requested);
    } else {
        // This scheduler seems to be shut down, so return an
        // empty batch of work.
        return {};
    }
}

} // namespace cask::scheduler