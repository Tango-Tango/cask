//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Task.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::Task;
using cask::scheduler::BenchScheduler;

TEST(TaskFlatMapError, PureValue) {
    auto result_opt = Task<int,std::string>::pure(123)
        .template flatMapError<std::string>([](auto) {
            return Task<int,std::string>::raiseError("broke");
        })
        .runSync();

    ASSERT_TRUE(result_opt.has_value());
    ASSERT_TRUE(result_opt->is_left());
    EXPECT_EQ(result_opt->get_left(), 123);
}

TEST(TaskFlatMapError, ErrorToValue) {
    auto result_opt = Task<int,std::string>::raiseError("broke")
        .template flatMapError<std::string>([](auto) {
            return Task<int,std::string>::pure(123);
        })
        .runSync();

    ASSERT_TRUE(result_opt.has_value());
    ASSERT_TRUE(result_opt->is_left());
    EXPECT_EQ(result_opt->get_left(), 123);
}

TEST(TaskFlatMapError, ErrorToError) {
    auto result_opt = Task<int,std::string>::raiseError("broke")
        .template flatMapError<std::string>([](auto error) {
            return Task<int,std::string>::raiseError(error + " worse");
        })
        .runSync();

    ASSERT_TRUE(result_opt.has_value());
    ASSERT_TRUE(result_opt->is_right());
    EXPECT_EQ(result_opt->get_right(), std::string("broke worse"));
}

TEST(TaskFlatMapError, CancelsOuter) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Task<int,std::string>::never()
        .template flatMapError<std::string>([](auto error) {
            return Task<int,std::string>::raiseError(error + " worse");
        })
        .run(sched);

    sched->run_ready_tasks();
    fiber->cancel();
    sched->run_ready_tasks();

    ASSERT_TRUE(fiber->isCanceled());
}

TEST(TaskFlatMapError, CancelsInner) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Task<int,std::string>::raiseError("broke")
        .template flatMapError<std::string>([](auto) {
            return Task<int,std::string>::never();
        })
        .run(sched);

    sched->run_ready_tasks();
    fiber->cancel();
    sched->run_ready_tasks();

    ASSERT_TRUE(fiber->isCanceled());
}