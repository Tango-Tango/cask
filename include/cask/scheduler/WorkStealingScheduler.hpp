//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_WORK_STEALING_SCHEDULER_H_
#define _CASK_WORK_STEALING_SCHEDULER_H_

#include <atomic>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <thread>
#include <vector>

#include "../Scheduler.hpp"
#include "SingleThreadScheduler.hpp"

namespace cask::scheduler {

class WorkStealingScheduler final : public Scheduler, public std::enable_shared_from_this<WorkStealingScheduler> {
public:
    explicit WorkStealingScheduler(
        unsigned int poolSize = std::thread::hardware_concurrency(),
        std::optional<int> priority = std::nullopt
    );

    void submit(const std::function<void()>& task) override;
    void submitBulk(const std::vector<std::function<void()>>& tasks) override;
    CancelableRef submitAfter(int64_t milliseconds, const std::function<void()>& task) override;
    bool isIdle() const override;
    std::string toString() const override;

private:
    std::atomic_size_t running_thread_count;
    std::vector<std::thread::id> thread_ids;
    std::map<std::thread::id, std::shared_ptr<SingleThreadScheduler>> schedulers;

    static void onThreadIdle(const std::weak_ptr<WorkStealingScheduler>& self_weak);
    static void onThreadResume(const std::weak_ptr<WorkStealingScheduler>& self_weak);
    static std::vector<std::function<void()>> onThreadRequestWork(const std::weak_ptr<WorkStealingScheduler>& self_weak);
};

} // namespace cask::scheduler

#endif