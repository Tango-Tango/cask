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
    assert(poolSize > 0 && "Pool size must be greater than 0");
    auto idle_callback = std::bind(&WorkStealingScheduler::onThreadIdle, this->weak_from_this());
    auto resume_callback = std::bind(&WorkStealingScheduler::onThreadResume, this->weak_from_this());
    auto request_work_callback = std::bind(&WorkStealingScheduler::onThreadRequestWork, this->weak_from_this(), std::placeholders::_1);

    for(unsigned int i = 0; i < poolSize; i++) {
        auto sched = std::make_shared<SingleThreadScheduler>(priority, std::nullopt, batch_size, idle_callback, resume_callback, request_work_callback);
        auto thread_id = sched->get_run_thread_id();

        thread_ids.push_back(thread_id);
        schedulers[thread_id] = sched;
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
        // Select a random starting point for work stealing as a simple way to balance work
        // and avoid lock contention
        auto dice_roll = static_cast<std::size_t>(std::abs(std::rand()));
        auto random_start_index = dice_roll % self->thread_ids.size();
        auto i = random_start_index;

        while (true) {
            // Try and find a scheduler to steal work from
            auto thread_id = self->thread_ids[i];

            // Skip the current thread (the scheduler thread) to avoid attempting to
            // steal work from the scheduler requesting the work in the first place
            if (thread_id != current_thread_id) {
                // Check if the scheduler exists. If it doesn't then just give up since
                // we are probably shutting down.
                auto scheduler_lookup = self->schedulers.find(thread_id);
                if (scheduler_lookup == self->schedulers.end()) {
                    break;
                }

                // Try and steal some work
                auto batch = scheduler_lookup->second->steal(amount_requested);

                // If a batch was stolen, submit it to the idle scheduler
                if (batch.size() > 0) {
                    return batch;
                }
            }

            // Move to the next thread - giving up if we've already
            // checked all threads
            i = (i + 1) % self->thread_ids.size();
            if (i == random_start_index) {
                break;
            }
        }
    }

    return std::vector<std::function<void()>>();
}

} // namespace cask::scheduler