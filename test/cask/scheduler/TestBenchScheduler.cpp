//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/scheduler/BenchScheduler.hpp"
#include "gtest/gtest.h"

using cask::scheduler::BenchScheduler;

TEST(BenchScheduler, ConstructsEmpty) {
    auto sched = std::make_shared<BenchScheduler>();
    EXPECT_EQ(sched->num_task_ready(), 0);
    EXPECT_EQ(sched->num_timers(), 0);
    EXPECT_FALSE(sched->run_one_task());
    EXPECT_EQ(sched->run_ready_tasks(), 0);
    EXPECT_TRUE(sched->isIdle());
}

TEST(BenchScheduler, SubmitAndRunOne) {
    auto sched = std::make_shared<BenchScheduler>();

    int counter = 0;
    sched->submit([&counter] { counter++; });

    EXPECT_EQ(sched->num_task_ready(), 1);
    EXPECT_FALSE(sched->isIdle());
    EXPECT_EQ(counter, 0);
    
    EXPECT_TRUE(sched->run_one_task());
    EXPECT_FALSE(sched->run_one_task());

    EXPECT_TRUE(sched->isIdle());
    EXPECT_EQ(sched->num_task_ready(), 0);
    EXPECT_EQ(counter, 1);
}

TEST(BenchScheduler, SubmitAndRunReady) {
    auto sched = std::make_shared<BenchScheduler>();

    int counter = 0;
    sched->submit([&counter] { counter++; });

    EXPECT_EQ(sched->num_task_ready(), 1);
    EXPECT_FALSE(sched->isIdle());
    EXPECT_EQ(counter, 0);

    EXPECT_EQ(sched->run_ready_tasks(), 1);
    EXPECT_EQ(sched->run_ready_tasks(), 0);

    EXPECT_TRUE(sched->isIdle());
    EXPECT_EQ(sched->num_task_ready(), 0);
    EXPECT_EQ(counter, 1);
}

TEST(BenchScheduler, SubmitAndRunMultipleReady) {
    auto sched = std::make_shared<BenchScheduler>();

    int counter = 0;
    sched->submit([&counter] { counter++; });
    sched->submit([&counter] { counter++; });
    sched->submit([&counter] { counter++; });

    EXPECT_FALSE(sched->isIdle());
    EXPECT_EQ(sched->num_task_ready(), 3);
    EXPECT_EQ(counter, 0);

    EXPECT_EQ(sched->run_ready_tasks(), 3);
    EXPECT_EQ(sched->run_ready_tasks(), 0);

    EXPECT_TRUE(sched->isIdle());
    EXPECT_EQ(sched->num_task_ready(), 0);
    EXPECT_EQ(counter, 3);
}

TEST(BenchScheduler, SubmitBulk) {
    auto sched = std::make_shared<BenchScheduler>();

    int counter = 0;
    std::vector<std::function<void()>> tasks = {
        [&counter] { counter++; },
        [&counter] { counter++; },
        [&counter] { counter++; }
    };

    sched->submitBulk(tasks);

    EXPECT_FALSE(sched->isIdle());
    EXPECT_EQ(sched->num_task_ready(), 3);
    EXPECT_EQ(counter, 0);

    EXPECT_EQ(sched->run_ready_tasks(), 3);
    EXPECT_EQ(sched->run_ready_tasks(), 0);

    EXPECT_TRUE(sched->isIdle());
    EXPECT_EQ(sched->num_task_ready(), 0);
    EXPECT_EQ(counter, 3);
}

TEST(BenchScheduler, SubmitAfterAndAdvanceTime) {
    auto sched = std::make_shared<BenchScheduler>();

    int counter = 0;
    sched->submitAfter(10, [&counter] { counter++; });

    EXPECT_FALSE(sched->isIdle());
    EXPECT_EQ(sched->num_task_ready(), 0);
    EXPECT_EQ(sched->num_timers(), 1);
    EXPECT_FALSE(sched->run_one_task());
    EXPECT_EQ(counter, 0);

    sched->advance_time(9);

    EXPECT_FALSE(sched->isIdle());
    EXPECT_EQ(sched->num_task_ready(), 0);
    EXPECT_EQ(sched->num_timers(), 1);
    EXPECT_FALSE(sched->run_one_task());
    EXPECT_EQ(counter, 0);

    sched->advance_time(1);

    EXPECT_FALSE(sched->isIdle());
    EXPECT_EQ(sched->num_task_ready(), 1);
    EXPECT_EQ(sched->num_timers(), 0);
    EXPECT_TRUE(sched->run_one_task());
    EXPECT_TRUE(sched->isIdle());
    EXPECT_EQ(counter, 1);
}

TEST(BenchScheduler, SubmitAfterAndCancel) {
    auto sched = std::make_shared<BenchScheduler>();

    int timer_counter = 0;
    int cancel_counter = 0;

    auto handle = sched->submitAfter(10, [&timer_counter] { timer_counter++; });
    handle->onCancel([&cancel_counter] { cancel_counter++; });

    EXPECT_FALSE(sched->isIdle());
    EXPECT_EQ(sched->num_task_ready(), 0);
    EXPECT_EQ(sched->num_timers(), 1);

    handle->cancel();

    EXPECT_TRUE(sched->isIdle());
    EXPECT_EQ(sched->num_task_ready(), 0);
    EXPECT_EQ(sched->num_timers(), 0);
    EXPECT_EQ(timer_counter, 0);
    EXPECT_EQ(cancel_counter, 1);
}

TEST(BenchScheduler, SubmitAfterMultipleTimersCancelOne) {
    auto sched = std::make_shared<BenchScheduler>();

    int timer_counter = 0;
    int cancel_counter = 0;

    auto firstHandle = sched->submitAfter(10, [&timer_counter] { timer_counter++; });
    firstHandle->onCancel([&cancel_counter] { cancel_counter++; });

    auto secondHandle = sched->submitAfter(10, [&timer_counter] { timer_counter++; });
    secondHandle->onCancel([&cancel_counter] { cancel_counter++; });

    firstHandle->cancel();
    EXPECT_EQ(timer_counter, 0);
    EXPECT_EQ(cancel_counter, 1);

    sched->advance_time(10);
    sched->run_ready_tasks();
    EXPECT_EQ(timer_counter, 1);
    EXPECT_EQ(cancel_counter, 1);
}

TEST(BenchScheduler, SubmitAfterMultipleCancels) {
    auto sched = std::make_shared<BenchScheduler>();

    int timer_counter = 0;
    int cancel_counter = 0;

    auto handle = sched->submitAfter(10, [&timer_counter] { timer_counter++; });
    handle->onCancel([&cancel_counter] { cancel_counter++; });
    handle->cancel();
    handle->cancel();
    handle->cancel();

    EXPECT_TRUE(sched->isIdle());
    EXPECT_EQ(sched->num_task_ready(), 0);
    EXPECT_EQ(sched->num_timers(), 0);
    EXPECT_EQ(timer_counter, 0);
    EXPECT_EQ(cancel_counter, 1);
}

TEST(BenchScheduler, CancelsAfterTimerFires) {
    auto sched = std::make_shared<BenchScheduler>();

    int timer_counter = 0;
    int cancel_counter = 0;

    auto handle = sched->submitAfter(10, [&timer_counter] { timer_counter++; });
    handle->onCancel([&cancel_counter] { cancel_counter++; });

    sched->advance_time(10);
    handle->cancel();

    EXPECT_FALSE(sched->isIdle());
    EXPECT_EQ(sched->num_task_ready(), 1);
    EXPECT_EQ(sched->num_timers(), 0);
    EXPECT_EQ(cancel_counter, 0);
    EXPECT_EQ(timer_counter, 0);

    sched->run_ready_tasks();
    EXPECT_TRUE(sched->isIdle());
    EXPECT_EQ(cancel_counter, 0);
    EXPECT_EQ(timer_counter, 1);
}

TEST(BenchScheduler, RegistersCallbackAfterCanceled) {
    auto sched = std::make_shared<BenchScheduler>();

    int timer_counter = 0;
    int cancel_counter = 0;

    auto handle = sched->submitAfter(10, [&timer_counter] { timer_counter++; });
    handle->cancel();
    handle->onCancel([&cancel_counter] { cancel_counter++; });

    EXPECT_TRUE(sched->isIdle());
    EXPECT_EQ(sched->num_task_ready(), 0);
    EXPECT_EQ(sched->num_timers(), 0);
    EXPECT_EQ(timer_counter, 0);
    EXPECT_EQ(cancel_counter, 1);
}

TEST(BenchScheduler, RunsShutdownCallbackAfterTimerTaskCompletion) {
    auto sched = std::make_shared<BenchScheduler>();

    int timer_counter = 0;
    int shutdown_counter = 0;

    auto handle = sched->submitAfter(10, [&timer_counter] { timer_counter++; });
    handle->onShutdown([&shutdown_counter] { shutdown_counter++; });

    sched->advance_time(10);

    EXPECT_FALSE(sched->isIdle());
    EXPECT_EQ(sched->num_task_ready(), 1);
    EXPECT_EQ(sched->num_timers(), 0);
    EXPECT_EQ(shutdown_counter, 0);
    EXPECT_EQ(timer_counter, 0);

    sched->run_ready_tasks();
    EXPECT_TRUE(sched->isIdle());
    EXPECT_EQ(shutdown_counter, 1);
    EXPECT_EQ(timer_counter, 1);
}

TEST(BenchScheduler, RunsShutdownImmediatelyCallbackIfTimerAlreadyFired) {
    auto sched = std::make_shared<BenchScheduler>();

    int timer_counter = 0;
    int shutdown_counter = 0;

    auto handle = sched->submitAfter(10, [&timer_counter] { timer_counter++; });

    sched->advance_time(10);
    sched->run_ready_tasks();
    EXPECT_TRUE(sched->isIdle());
    EXPECT_EQ(shutdown_counter, 0);
    EXPECT_EQ(timer_counter, 1);

    handle->onShutdown([&shutdown_counter] { shutdown_counter++; });
    EXPECT_EQ(shutdown_counter, 1);
}

