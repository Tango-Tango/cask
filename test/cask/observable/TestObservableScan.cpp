//          Copyright Tango Tango, Inc. 2020 - 2022.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::Observable;
using cask::scheduler::BenchScheduler;

TEST(ObservableScan, Pure) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::pure(123)
        ->scan<int>(0, [](auto acc, auto value) {
            return acc + value;
        })
        ->last()
        .run(sched);
    
    sched->run_ready_tasks();

    auto result = fiber->await();

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123); // NOLINT(bugprone-unchecked-optional-access)
}

TEST(ObservableScan, Vector) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::sequence(1,2,3,4,5)
        ->scan<int>(0, [](auto acc, auto value) {
            return acc + value;
        })
        ->take(6)
        .run(sched);
    
    sched->run_ready_tasks();

    auto result = fiber->await();

    ASSERT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 3);
    EXPECT_EQ(result[2], 6);
    EXPECT_EQ(result[3], 10);
    EXPECT_EQ(result[4], 15);
}

TEST(ObservableScan, Empty) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::empty()
        ->scan<int>(0, [](auto acc, auto value) {
            return acc + value;
        })
        ->last()
        .run(sched);
    
    sched->run_ready_tasks();

    auto result = fiber->await();

    EXPECT_FALSE(result.has_value());
}

TEST(ObservableScan, Stop) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::sequence(1,2,3,4,5)
        ->scan<int>(0, [](auto acc, auto value) {
            return acc + value;
        })
        ->take(2)
        .run(sched);
    
    sched->run_ready_tasks();

    auto result = fiber->await();

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 3);
}

TEST(ObservableScan, Never) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::never()
        ->scan<int>(0, [](auto acc, auto value) {
            return acc + value;
        })
        ->take(2)
        .run(sched);
    
    sched->run_ready_tasks();

    EXPECT_FALSE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_FALSE(fiber->isCanceled());

    fiber->cancel();
    sched->run_ready_tasks();
    EXPECT_TRUE(fiber->isCanceled());
}

TEST(ObservableScan, Error) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int, std::string>::raiseError("broke")
        ->scan<int>(0, [](auto acc, auto value) {
            return acc + value;
        })
        ->last()
        .failed()
        .run(sched);
    
    sched->run_ready_tasks();

    auto result = fiber->await();

    EXPECT_EQ(result, "broke");
}
