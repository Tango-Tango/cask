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

TEST(TaskDoOnCancel, IgnoredForCompletedValue) {
    int cancel_counter = 0;
    auto sched = std::make_shared<BenchScheduler>();

    auto fiber = Task<int,std::string>::pure(123)
        .doOnCancel(Task<None,None>::eval([&cancel_counter] {
            cancel_counter++;
            return None();
        }))
        .run(sched);

    sched->run_ready_tasks();
    fiber->cancel();
    sched->run_ready_tasks();

    ASSERT_TRUE(fiber->getValue().has_value());
    EXPECT_EQ(*(fiber->getValue()), 123);
    EXPECT_EQ(cancel_counter, 0);
}

TEST(TaskDoOnCancel, IgnoredForCompletedError) {
    int cancel_counter = 0;
    auto sched = std::make_shared<BenchScheduler>();

    auto fiber = Task<int,std::string>::raiseError("broke")
        .doOnCancel(Task<None,None>::eval([&cancel_counter] {
            cancel_counter++;
            return None();
        }))
        .run(sched);

    sched->run_ready_tasks();
    fiber->cancel();
    sched->run_ready_tasks();

    ASSERT_TRUE(fiber->getError().has_value());
    EXPECT_EQ(*(fiber->getError()), "broke");
    EXPECT_EQ(cancel_counter, 0);
}

TEST(TaskDoOnCancel, IgnoredForEarlyCancelFire) {
    int cancel_counter = 0;
    auto sched = std::make_shared<BenchScheduler>();

    auto fiber = Task<int,std::string>::pure(123)
        .doOnCancel(Task<None,None>::eval([&cancel_counter] {
            cancel_counter++;
            return None();
        }))
        .run(sched);

    fiber->cancel();
    sched->run_ready_tasks();

    ASSERT_TRUE(fiber->isCanceled());
    EXPECT_EQ(cancel_counter, 0);
}


TEST(TaskDoOnCancel, RunsForCanceledTask) {
    int cancel_counter = 0;
    auto sched = std::make_shared<BenchScheduler>();

    auto fiber = Task<int,std::string>::never()
        .doOnCancel(Task<None,None>::eval([&cancel_counter] {
            cancel_counter++;
            return None();
        }))
        .run(sched);

    sched->run_ready_tasks();
    fiber->cancel();
    sched->run_ready_tasks();

    ASSERT_TRUE(fiber->isCanceled());
    EXPECT_EQ(cancel_counter, 1);
}

