//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/Task.hpp"
#include "cask/scheduler/BenchScheduler.hpp"
#include "gtest/gtest.h"

using cask::None;
using cask::Task;
using cask::scheduler::BenchScheduler;

TEST(TestTaskDelay, DelaysExecution) {
    int counter = 0;
    auto sched = std::make_shared<BenchScheduler>();

    auto fiber = Task<int>::eval([&counter] {
                     return counter++;
                 })
                     .delay(10)
                     .run(sched);

    EXPECT_EQ(counter, 0);

    sched->run_ready_tasks();
    sched->advance_time(9);
    sched->run_ready_tasks();
    EXPECT_EQ(counter, 0);

    sched->advance_time(1);
    sched->run_ready_tasks();
    EXPECT_EQ(counter, 1);
    sched->run_ready_tasks();
    EXPECT_TRUE(sched->isIdle());
}

TEST(TestTaskDelay, CancelsExecution) {
    int counter = 0;
    auto sched = std::make_shared<BenchScheduler>();

    auto fiber = Task<int>::eval([&counter] {
                     return counter++;
                 })
                     .delay(10)
                     .run(sched);

    sched->run_ready_tasks();
    EXPECT_EQ(sched->num_timers(), 1);

    fiber->cancel();
    sched->run_ready_tasks();
    EXPECT_EQ(sched->num_timers(), 0);
    EXPECT_TRUE(fiber->isCanceled());
}
