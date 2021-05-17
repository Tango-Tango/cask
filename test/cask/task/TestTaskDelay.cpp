//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Task.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::None;
using cask::Task;
using cask::scheduler::BenchScheduler;

TEST(TestTaskDelay, DelaysExecution) {
    int counter = 0;
    auto sched = std::make_shared<BenchScheduler>();

    auto deferred = Task<int>::eval([&counter] { return counter++; })
        .delay(10)
        .run(sched);

    EXPECT_EQ(counter, 0);

    sched->advance_time(9);
    sched->run_ready_tasks();
    EXPECT_EQ(counter, 0);

    sched->advance_time(1);
    sched->run_ready_tasks();
    EXPECT_EQ(counter, 1);
}

TEST(TestTaskDelay, CancelsExecution) {
    int counter = 0;
    auto sched = std::make_shared<BenchScheduler>();

    auto deferred = Task<int>::eval([&counter] { return counter++; })
        .delay(10)
        .run(sched);

    EXPECT_EQ(sched->num_timers(), 1);
    deferred->cancel();
    EXPECT_EQ(sched->num_timers(), 0);
}
