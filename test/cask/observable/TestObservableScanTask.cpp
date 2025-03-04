//          Copyright Tango Tango, Inc. 2020 - 2022.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::Observable;
using cask::Task;
using cask::scheduler::BenchScheduler;

TEST(ObservableScanTask, Pure) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::pure(123)
        ->scanTask<int>(0, [](auto acc, auto value) {
            return Task<int>::pure(acc + value);
        })
        ->last()
        .run(sched);
    
    sched->run_ready_tasks();

    auto result = fiber->await();

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);  // NOLINT(bugprone-unchecked-optional-access)
}

TEST(ObservableScanTask, Vector) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::sequence(
            1,2,3,4,5
        )
        ->scanTask<int>(0, [](auto acc, auto value) {
            return Task<int>::pure(acc + value);
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

TEST(ObservableScanTask, Empty) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::empty()
        ->scanTask<int>(0, [](auto acc, auto value) {
            return Task<int>::pure(acc + value);
        })
        ->last()
        .run(sched);
    
    sched->run_ready_tasks();

    auto result = fiber->await();

    EXPECT_FALSE(result.has_value());
}

TEST(ObservableScanTask, Stop) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::sequence(
            1,2,3,4,5
        )
        ->scanTask<int>(0, [](auto acc, auto value) {
            return Task<int>::pure(acc + value);
        })
        ->take(2)
        .run(sched);
    
    sched->run_ready_tasks();

    auto result = fiber->await();

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 3);
}

TEST(ObservableScanTask, Never) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::never()
        ->scanTask<int>(0, [](auto acc, auto value) {
            return Task<int>::pure(acc + value);
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

TEST(ObservableScanTask, NeverInner) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::pure(123)
        ->scanTask<int>(0, [](auto, auto) {
            return Task<int>::never();
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

TEST(ObservableScanTask, Error) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int, std::string>::raiseError("broke")
        ->scanTask<int>(0, [](auto acc, auto value) {
            return Task<int,std::string>::pure(acc + value);
        })
        ->last()
        .failed()
        .run(sched);
    
    sched->run_ready_tasks();

    auto result = fiber->await();

    EXPECT_EQ(result, "broke");
}

TEST(ObservableScanTask, ErrorInner) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int, std::string>::pure(123)
        ->scanTask<int>(0, [](auto, auto) {
            return Task<int,std::string>::raiseError("broke");
        })
        ->last()
        .failed()
        .run(sched);
    
    sched->run_ready_tasks();

    auto result = fiber->await();

    EXPECT_EQ(result, "broke");
}
