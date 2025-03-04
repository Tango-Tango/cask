//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Task.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::Task;
using cask::scheduler::BenchScheduler;

// NOLINTBEGIN(bugprone-unchecked-optional-access)

TEST(TaskRecover, PureValue) {
    auto result_opt = Task<int,std::string>::pure(123)
        .recover([](auto) {
            return 456;
        })
        .runSync();

    ASSERT_TRUE(result_opt.has_value());
    ASSERT_TRUE(result_opt->is_left());
    EXPECT_EQ(result_opt->get_left(), 123);
}

TEST(TaskRecover, Error) {
    auto result_opt = Task<int,std::string>::raiseError("broke")
        .recover([](auto) {
            return 456;
        })
        .runSync();

    ASSERT_TRUE(result_opt.has_value());
    ASSERT_TRUE(result_opt->is_left());
    EXPECT_EQ(result_opt->get_left(), 456);
}

TEST(TaskRecover, Canceled) {
    auto sched = std::make_shared<BenchScheduler>();

    auto fiber = Task<int,std::string>::never()
        .recover([](auto) {
            return 456;
        })
        .run(sched);

    sched->run_ready_tasks();
    fiber->cancel();
    sched->run_ready_tasks();

    try {
        fiber->await();
        FAIL() << "Expected call to throw.";
    } catch(std::runtime_error&) {
        SUCCEED();
    }
}

// NOLINTEND(bugprone-unchecked-optional-access)
