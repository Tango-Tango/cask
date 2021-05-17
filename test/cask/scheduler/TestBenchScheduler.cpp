//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/scheduler/BenchScheduler.hpp"
#include "gtest/gtest.h"

using cask::scheduler::BenchScheduler;

TEST(BenchScheduler, ConstructsEmpty) {
    BenchScheduler sched;
    EXPECT_EQ(sched.num_task_ready(), 0);
    EXPECT_EQ(sched.num_timers(), 0);
    EXPECT_FALSE(sched.run_one_task());
    EXPECT_EQ(sched.run_ready_tasks(), 0);
    EXPECT_TRUE(sched.isIdle());
}

TEST(BenchScheduler, SubmitAndRunOne) {
    BenchScheduler sched;

    int counter = 0;
    sched.submit([&counter] { counter++; });

    EXPECT_EQ(sched.num_task_ready(), 1);
    EXPECT_FALSE(sched.isIdle());
    EXPECT_EQ(counter, 0);
    
    EXPECT_TRUE(sched.run_one_task());
    EXPECT_FALSE(sched.run_one_task());

    EXPECT_TRUE(sched.isIdle());
    EXPECT_EQ(sched.num_task_ready(), 0);
    EXPECT_EQ(counter, 1);
}

TEST(BenchScheduler, SubmitAndRunReady) {
    BenchScheduler sched;

    int counter = 0;
    sched.submit([&counter] { counter++; });

    EXPECT_EQ(sched.num_task_ready(), 1);
    EXPECT_FALSE(sched.isIdle());
    EXPECT_EQ(counter, 0);

    EXPECT_EQ(sched.run_ready_tasks(), 1);
    EXPECT_EQ(sched.run_ready_tasks(), 0);

    EXPECT_TRUE(sched.isIdle());
    EXPECT_EQ(sched.num_task_ready(), 0);
    EXPECT_EQ(counter, 1);
}

TEST(BenchScheduler, SubmitAndRunMultipleReady) {
    BenchScheduler sched;

    int counter = 0;
    sched.submit([&counter] { counter++; });
    sched.submit([&counter] { counter++; });
    sched.submit([&counter] { counter++; });

    EXPECT_FALSE(sched.isIdle());
    EXPECT_EQ(sched.num_task_ready(), 3);
    EXPECT_EQ(counter, 0);

    EXPECT_EQ(sched.run_ready_tasks(), 3);
    EXPECT_EQ(sched.run_ready_tasks(), 0);

    EXPECT_TRUE(sched.isIdle());
    EXPECT_EQ(sched.num_task_ready(), 0);
    EXPECT_EQ(counter, 3);
}

TEST(BenchScheduler, SubmitBulk) {
    BenchScheduler sched;

    int counter = 0;
    std::vector<std::function<void()>> tasks = {
        [&counter] { counter++; },
        [&counter] { counter++; },
        [&counter] { counter++; }
    };

    sched.submitBulk(tasks);

    EXPECT_FALSE(sched.isIdle());
    EXPECT_EQ(sched.num_task_ready(), 3);
    EXPECT_EQ(counter, 0);

    EXPECT_EQ(sched.run_ready_tasks(), 3);
    EXPECT_EQ(sched.run_ready_tasks(), 0);

    EXPECT_TRUE(sched.isIdle());
    EXPECT_EQ(sched.num_task_ready(), 0);
    EXPECT_EQ(counter, 3);
}

TEST(BenchScheduler, SubmitAfterAndAdvanceTime) {
    BenchScheduler sched;

    int counter = 0;
    sched.submitAfter(10, [&counter] { counter++; });

    EXPECT_FALSE(sched.isIdle());
    EXPECT_EQ(sched.num_task_ready(), 0);
    EXPECT_EQ(sched.num_timers(), 1);
    EXPECT_FALSE(sched.run_one_task());
    EXPECT_EQ(counter, 0);

    sched.advance_time(9);

    EXPECT_FALSE(sched.isIdle());
    EXPECT_EQ(sched.num_task_ready(), 0);
    EXPECT_EQ(sched.num_timers(), 1);
    EXPECT_FALSE(sched.run_one_task());
    EXPECT_EQ(counter, 0);

    sched.advance_time(1);

    EXPECT_FALSE(sched.isIdle());
    EXPECT_EQ(sched.num_task_ready(), 1);
    EXPECT_EQ(sched.num_timers(), 0);
    EXPECT_TRUE(sched.run_one_task());
    EXPECT_TRUE(sched.isIdle());
    EXPECT_EQ(counter, 1);
}

