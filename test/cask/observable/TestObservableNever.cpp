//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/Observable.hpp"
#include "cask/scheduler/BenchScheduler.hpp"
#include "gtest/gtest.h"

using cask::Observable;
using cask::scheduler::BenchScheduler;

TEST(ObservableNever, NeverCompletes) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int, std::string>::never()->last().run(sched);

    sched->run_ready_tasks();

    EXPECT_FALSE(fiber->isCanceled());
    EXPECT_FALSE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());

    EXPECT_TRUE(sched->isIdle());
    EXPECT_EQ(sched->num_timers(), 0);
}

TEST(ObservableNever, Cancels) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int, std::string>::never()->last().run(sched);

    sched->run_ready_tasks();
    fiber->cancel();
    sched->run_ready_tasks();

    EXPECT_TRUE(fiber->isCanceled());
    EXPECT_FALSE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());

    EXPECT_TRUE(sched->isIdle());
    EXPECT_EQ(sched->num_timers(), 0);
}
