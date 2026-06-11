//          Copyright Tango Tango, Inc. 2020 - 2025.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>
#include "gtest/gtest.h"
#include "cask/scheduler/SingleThreadScheduler.hpp"

using cask::scheduler::SingleThreadScheduler;

TEST(SingleThreadSchedulerTest, StopsCleanlyWhenNeverStarted) {
    // A scheduler constructed with DisableAutoStart that is never started
    // must still shut down cleanly (and not leak its run thread) when
    // destructed.
    auto sched = std::make_unique<SingleThreadScheduler>(
        std::nullopt,
        std::nullopt,
        std::nullopt,
        [](){},
        [](){},
        [](auto&){},
        [](auto&){},
        cask::scheduler::DisableAutoStart);

    sched.reset();
}

TEST(SingleThreadSchedulerTest, ExplicitStopWhenNeverStarted) {
    auto sched = std::make_unique<SingleThreadScheduler>(
        std::nullopt,
        std::nullopt,
        std::nullopt,
        [](){},
        [](){},
        [](auto&){},
        [](auto&){},
        cask::scheduler::DisableAutoStart);

    sched->stop();
    sched.reset();
}

TEST(SingleThreadSchedulerTest, ReleasesRunThreadWhenNeverStarted) {
    // The run thread holds a reference to the scheduler's internal control
    // data (which owns the callbacks given here). If the scheduler is
    // destructed without ever being started, the run thread must still be
    // woken so it can exit and release that reference rather than leak.
    auto sentinel = std::make_shared<int>(0);
    std::weak_ptr<int> watcher = sentinel;

    {
        auto sched = std::make_unique<SingleThreadScheduler>(
            std::nullopt,
            std::nullopt,
            std::nullopt,
            [sentinel](){},
            [](){},
            [](auto&){},
            [](auto&){},
            cask::scheduler::DisableAutoStart);

        sentinel.reset();
    }

    // After destruction the run thread is the only remaining owner of the
    // sentinel. Wait for it to exit and drop its reference.
    bool released = false;
    for (int i = 0; i < 1000; i++) {
        if (watcher.expired()) {
            released = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_TRUE(released);
}

TEST(SingleThreadSchedulerTest, StartAfterStopDoesNotHang) {
    // If stop() runs before start(), the run thread exits without ever
    // setting thread_running. A subsequent start() must observe the
    // shutdown and return rather than spin forever.
    auto sched = std::make_unique<SingleThreadScheduler>(
        std::nullopt,
        std::nullopt,
        std::nullopt,
        [](){},
        [](){},
        [](auto&){},
        [](auto&){},
        cask::scheduler::DisableAutoStart);

    sched->stop();
    sched->start();
    sched.reset();
}

TEST(SingleThreadSchedulerTest, FiresExpiredTimersInSubmissionOrder) {
    // When multiple timers expire before the run loop evaluates them
    // (e.g. while the scheduler was busy or not yet started), they must
    // fire oldest-first rather than in reverse order.
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<int> fired_order;

    auto record = [&](int id) {
        std::lock_guard<std::mutex> lock(mutex);
        fired_order.push_back(id);
        cv.notify_one();
    };

    auto sched = std::make_unique<SingleThreadScheduler>(
        std::nullopt,
        std::nullopt,
        std::nullopt,
        [](){},
        [](){},
        [](auto&){},
        [](auto&){},
        cask::scheduler::DisableAutoStart);

    // Submit timers with strictly increasing expiration times so each
    // lands in its own timer bucket.
    sched->submitAfter(5,  [&] { record(0); });
    sched->submitAfter(10, [&] { record(1); });
    sched->submitAfter(15, [&] { record(2); });

    // Let all three timers expire before the scheduler ever runs, so a
    // single evaluation pass sees them all at once.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    sched->start();

    {
        std::unique_lock<std::mutex> lock(mutex);
        auto fired = cv.wait_for(lock, std::chrono::seconds(5), [&] {
            return fired_order.size() == 3;
        });

        ASSERT_TRUE(fired);
        EXPECT_EQ(fired_order, (std::vector<int>{0, 1, 2}));
    }

    sched->stop();
    sched.reset();
}

TEST(SingleThreadSchedulerTest, StartsAndStops) {
    auto sched = std::make_unique<SingleThreadScheduler>(
        std::nullopt,
        std::nullopt,
        std::nullopt,
        [](){},
        [](){},
        [](auto&){},
        [](auto&){},
        cask::scheduler::DisableAutoStart);

    sched->start();
    sched->stop();
    sched.reset();
}
