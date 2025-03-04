//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Task.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::Task;
using cask::scheduler::BenchScheduler;

TEST(TaskDeferFiber, PureValue) {
    auto sched = std::make_shared<BenchScheduler>();

    auto fiber = Task<int,std::string>::deferFiber([](auto sched) {
            return Task<int,std::string>::pure(123).run(sched);
        })
        .run(sched);

    sched->run_ready_tasks();

    ASSERT_TRUE(fiber->getValue().has_value());
    EXPECT_EQ(*(fiber->getValue()), 123);  // NOLINT(bugprone-unchecked-optional-access)
}

TEST(TaskDeferFiber, Error) {
    auto sched = std::make_shared<BenchScheduler>();

    auto fiber = Task<int,std::string>::deferFiber([](auto sched) {
            return Task<int,std::string>::raiseError("broke").run(sched);
        })
        .run(sched);

    sched->run_ready_tasks();

    ASSERT_TRUE(fiber->getError().has_value());
    EXPECT_EQ(*(fiber->getError()), "broke");  // NOLINT(bugprone-unchecked-optional-access)
}

TEST(TaskDeferFiber, Cancels) {
    auto sched = std::make_shared<BenchScheduler>();

    auto fiber = Task<int,std::string>::deferFiber([](auto sched) {
            return Task<int,std::string>::never().run(sched);
        })
        .run(sched);

    sched->run_ready_tasks();
    fiber->cancel();
    sched->run_ready_tasks();

    ASSERT_TRUE(fiber->isCanceled());
}

