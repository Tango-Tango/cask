//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)


#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::Observable;
using cask::scheduler::BenchScheduler;

TEST(ObervableMerge,Empty) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int,std::string>::empty()
        ->merge(Observable<int,std::string>::empty())
        ->last()
        .run(sched);

    sched->run_ready_tasks();

    auto result = fiber->await();
    EXPECT_FALSE(result.has_value());

    EXPECT_TRUE(sched->isIdle());
    EXPECT_EQ(sched->num_timers(), 0);
}
