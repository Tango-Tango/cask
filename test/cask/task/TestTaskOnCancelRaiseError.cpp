//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Task.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::Task;
using cask::scheduler::BenchScheduler;

TEST(TaskOnCancelRaiseError,ConvertsToError) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Task<int, std::string>::never()
        .onCancelRaiseError("cancel happened")
        .failed()
        .run(sched);

    sched->run_ready_tasks();
    fiber->cancel();
    sched->run_ready_tasks();

    EXPECT_EQ(fiber->await(), "cancel happened");
}

TEST(TaskOnCancelRaiseError,IgnoresValue) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Task<int, std::string>::pure(123)
        .onCancelRaiseError("cancel happened")
        .run(sched);

    sched->run_ready_tasks();
    fiber->cancel();
    sched->run_ready_tasks();

    EXPECT_EQ(fiber->await(), 123);
}

TEST(TaskOnCancelRaiseError,IgnoresError) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Task<int, std::string>::raiseError("broke")
        .onCancelRaiseError("cancel happened")
        .failed()
        .run(sched);

    sched->run_ready_tasks();
    fiber->cancel();
    sched->run_ready_tasks();

    EXPECT_EQ(fiber->await(), "broke");
}
