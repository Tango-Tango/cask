//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_WORK_STEALING_SCHEDULER_H_
#define _CASK_WORK_STEALING_SCHEDULER_H_

#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <thread>
#include <vector>

#include "../Scheduler.hpp"
#include "SingleThreadScheduler.hpp"

namespace cask::scheduler {

class WorkStealingScheduler final : public Scheduler, public std::enable_shared_from_this<WorkStealingScheduler> {
public:
    explicit WorkStealingScheduler(unsigned int poolSize = std::thread::hardware_concurrency(), int priority = 0);

    void submit(const std::function<void()>& task) override;
    void submitBulk(const std::vector<std::function<void()>>& tasks) override;
    CancelableRef submitAfter(int64_t milliseconds, const std::function<void()>& task) override;
    bool isIdle() const override;

private:
    std::atomic_size_t runningThreadCount;
    std::vector<std::thread::id> threadIds;
    std::map<std::thread::id, std::shared_ptr<SingleThreadScheduler>> schedulers;

    static void onThreadIdle(std::weak_ptr<WorkStealingScheduler> self_weak);
    static void onThreadResume(std::weak_ptr<WorkStealingScheduler> self_weak);
    static std::vector<std::function<void()>> onThreadRequestWork(std::weak_ptr<WorkStealingScheduler> self_weak);
};

} // namespace cask::scheduler

#endif