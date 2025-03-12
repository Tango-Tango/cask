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

    ~WorkStealingScheduler();

    bool submit(const std::function<void()>& task) override;
    bool submitBulk(const std::vector<std::function<void()>>& tasks) override;
    CancelableRef submitAfter(int64_t milliseconds, const std::function<void()>& task) override;
    bool isIdle() const override;
    std::string toString() const override;

private:
    // Data that is safe to read concurrently without locking
    // because it is initialized once and never modified
    //
    // NOTE: The guarantee here is that these structures will
    //       not mutate and the references contained within
    //       them will not change. The schedulers stored may
    //       themselves mutate - but their APIs are thread-safe.
    struct FixedData {
        FixedData();
        std::vector<std::shared_ptr<SingleThreadScheduler>> schedulers;
        std::vector<std::thread::id> thread_ids;
        std::map<std::thread::id, std::size_t> thread_id_indexes;
        std::map<std::thread::id, std::shared_ptr<SingleThreadScheduler>> schedulers_by_thread_id;
    };

    // Data that must be locked (via the contained mutex) before it is safe
    // to read or write
    struct SychronizedData {
        SychronizedData();
        std::mutex mutex;
        std::deque<std::function<void()>> global_ready_queue;
    };

    struct SchedulerData {
        SchedulerData();

        // An atomic count of the number of remaining running threads
        std::atomic_size_t running_thread_count;

        // A reference to data that is safe to use concurrently read without locking
        FixedData fixed;

        // Data that must be locked (via the contained mutex) before it is safe
        // to read or write
        SychronizedData synchronized;
    };

    std::shared_ptr<SchedulerData> data;


    // Handle thread idle callbacks from child schedulers
    static void on_thread_idle(const std::weak_ptr<SchedulerData>& data_weak);

    // Handle thread resume callbacks from child schedulers
    static void on_thread_resume(const std::weak_ptr<SchedulerData>& data_weak);

    // Handle work request (steal) callbacks from child schedulers
    static void on_thread_request_work(const std::weak_ptr<SchedulerData>& data_weak, ReadyQueue& requestor_ready_queue);

    // Handle work overflow callbacks from child schedulers
    static void on_work_overflow(const std::weak_ptr<SchedulerData>& data_weak, std::deque<std::function<void()>>& overflowed_tasks);

    // Drain from the global queue into the given ReadyQueue
    static bool drain_global_queue(const std::shared_ptr<SchedulerData>& data, ReadyQueue& requestor_ready_queue);

    // Steal from a random scheduler to populate the given ReadyQueue
    static bool steal_random_scheduler_unsafe(const std::shared_ptr<SchedulerData>& data, ReadyQueue& requestor_ready_queue);
};

} // namespace cask::scheduler

#endif