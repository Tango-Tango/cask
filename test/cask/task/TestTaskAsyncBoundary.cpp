//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Task.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::Task;
using cask::scheduler::BenchScheduler;

TEST(TaskAsyncBoundary, DefersExecutionToScheduler) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Task<int,std::string>::pure(123).asyncBoundary().run(sched);

    EXPECT_FALSE(fiber->getValue().has_value());
    EXPECT_FALSE(sched->isIdle());

    sched->run_ready_tasks();
    EXPECT_TRUE(sched->isIdle());
    EXPECT_TRUE(fiber->getValue().has_value());
    EXPECT_EQ(*(fiber->getValue()), 123);
}

TEST(TaskAsyncBoundary, AllowsCancelation) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Task<int,std::string>::pure(123).asyncBoundary().run(sched);

    fiber->cancel();
    sched->run_ready_tasks();

    EXPECT_TRUE(sched->isIdle());
    EXPECT_TRUE(fiber->isCanceled());
}

