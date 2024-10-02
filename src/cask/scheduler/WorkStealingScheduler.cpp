//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/scheduler/WorkStealingScheduler.hpp"
#include <cstdlib>
#include <cassert>

namespace cask::scheduler {

WorkStealingScheduler::WorkStealingScheduler(unsigned int poolSize, int priority) {
    assert(poolSize > 0 && "Pool size must be greater than 0");
    auto idle_callback = std::bind(&WorkStealingScheduler::onThreadIdle, this->weak_from_this(), std::placeholders::_1);

    for(unsigned int i = 0; i < poolSize; i++) {
        auto sched = std::make_shared<SingleThreadScheduler>(priority, idle_callback);
        auto thread_id = sched->run_thread_id();

        threadIds.push_back(thread_id);
        schedulers[thread_id] = sched;
    }
}

void WorkStealingScheduler::submit(const std::function<void()>& task) {
    auto thread_handle = std::this_thread::get_id();
    auto scheduler_lookup = schedulers.find(thread_handle);

    if (scheduler_lookup != schedulers.end()) {
        scheduler_lookup->second->submit(task);
    } else {
        auto random_index = std::rand() % threadIds.size();
        auto random_thread_id = threadIds[random_index];
        schedulers[random_thread_id]->submit(task);
    }
}

void WorkStealingScheduler::submitBulk(const std::vector<std::function<void()>>& tasks) {
    auto thread_handle = std::this_thread::get_id();
    auto scheduler_lookup = schedulers.find(thread_handle);

    if (scheduler_lookup != schedulers.end()) {
        scheduler_lookup->second->submitBulk(tasks);
    } else {
        auto random_index = std::rand() % threadIds.size();
        auto random_thread_id = threadIds[random_index];
        schedulers[random_thread_id]->submitBulk(tasks);
    }
}

CancelableRef WorkStealingScheduler::submitAfter(int64_t milliseconds, const std::function<void()>& task) {
    auto thread_handle = std::this_thread::get_id();
    auto scheduler_lookup = schedulers.find(thread_handle);

    if (scheduler_lookup != schedulers.end()) {
        return scheduler_lookup->second->submitAfter(milliseconds, task);
    } else {
        auto random_index = std::rand() % threadIds.size();
        auto random_thread_id = threadIds[random_index];
        return schedulers[random_thread_id]->submitAfter(milliseconds, task);
    }
}

bool WorkStealingScheduler::isIdle() const {
    for(auto& [_, scheduler] : schedulers) {
        if (!scheduler->isIdle()) {
            return false;
        }
    }

    return true;
}

void WorkStealingScheduler::onThreadIdle(std::weak_ptr<WorkStealingScheduler> parent_scheduler, std::shared_ptr<SingleThreadScheduler> idle_scheduler) {
    if (auto parent = parent_scheduler.lock()) {
        auto random_start_index = std::size_t(std::abs(std::rand())) % parent->threadIds.size();
        auto i = random_start_index;

        while (true) {
            // Try and find a scheduler to steal work from
            auto thread_id = parent->threadIds[i];
            auto scheduler_lookup = parent->schedulers.find(thread_id);
            if (scheduler_lookup == parent->schedulers.end()) {
                return;
            }

            // Try and steal some work
            auto batch = scheduler_lookup->second->steal(128);

            // If a batch was stolen, submit it to the idle scheduler
            if (batch.size() > 0) {
                idle_scheduler->submitBulk(batch);
                return;
            }

            // Move to the next thread - giving up if we've already
            // checked all threads
            i = (i + 1) % parent->threadIds.size();
            if (i == random_start_index) {
                return;
            }
        }
    }
}

} // namespace cask::scheduler