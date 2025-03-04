//          Copyright Tango Tango, Inc. 2020 - 2022.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::Observable;
using cask::scheduler::BenchScheduler;

TEST(ObservableFlatScan, Pure) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::pure(123)
        ->flatScan<int>(0, [](auto acc, auto value) {
            return Observable<int>::pure(acc + value);
        })
        ->last()
        .run(sched);
    
    sched->run_ready_tasks();

    auto result = fiber->await();

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);  // NOLINT(bugprone-unchecked-optional-access)
}

TEST(ObservableFlatScan, Vector) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::sequence(1,2,3,4,5)
        ->flatScan<int>(0, [](auto acc, auto value) {
            return Observable<int>::pure(acc + value);
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

TEST(ObservableFlatScan, VectorInner) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::sequence(1, 2, 3)
        ->flatScan<int>(0, [](auto acc, auto value) {
            return Observable<int>::sequence(
                acc * value,
                (acc * value) + 1,
                (acc * value) + 2
            );
        })
        ->take(10)
        .run(sched);
    
    sched->run_ready_tasks();

    auto result = fiber->await();

    ASSERT_EQ(result.size(), 9);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 1);
    EXPECT_EQ(result[2], 2);

    EXPECT_EQ(result[3], 4);
    EXPECT_EQ(result[4], 5);
    EXPECT_EQ(result[5], 6);
    
    EXPECT_EQ(result[6], 18);
    EXPECT_EQ(result[7], 19);
    EXPECT_EQ(result[8], 20);
}

TEST(ObservableFlatScan, Empty) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::empty()
        ->flatScan<int>(0, [](auto acc, auto value) {
            return Observable<int>::pure(acc + value);
        })
        ->last()
        .run(sched);
    
    sched->run_ready_tasks();

    auto result = fiber->await();

    EXPECT_FALSE(result.has_value());
}

TEST(ObservableFlatScan, EmptyInner) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::pure(123)
        ->flatScan<int>(0, [](auto, auto) {
            return Observable<int>::empty();
        })
        ->last()
        .run(sched);
    
    sched->run_ready_tasks();

    auto result = fiber->await();

    EXPECT_FALSE(result.has_value());
}

TEST(ObservableFlatScan, Never) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::never()
        ->flatScan<int>(0, [](auto acc, auto value) {
            return Observable<int>::pure(acc + value);
        })
        ->last()
        .run(sched);
    
    sched->run_ready_tasks();

    EXPECT_FALSE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_FALSE(fiber->isCanceled());

    fiber->cancel();
    sched->run_ready_tasks();
    EXPECT_TRUE(fiber->isCanceled());
}

TEST(ObservableFlatScan, NeverInner) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::pure(123)
        ->flatScan<int>(0, [](auto, auto) {
            return Observable<int>::never();
        })
        ->last()
        .run(sched);
    
    sched->run_ready_tasks();

    EXPECT_FALSE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_FALSE(fiber->isCanceled());

    fiber->cancel();
    sched->run_ready_tasks();
    EXPECT_TRUE(fiber->isCanceled());
}

TEST(ObservableFlatScan, Error) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int,std::string>::raiseError("broke")
        ->flatScan<int>(0, [](auto acc, auto value) {
            return Observable<int, std::string>::pure(acc + value);
        })
        ->last()
        .failed()
        .run(sched);
    
    sched->run_ready_tasks();
    
    auto result = fiber->await();
    EXPECT_EQ(result, "broke");
}

TEST(ObservableFlatScan, InnerError) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int,std::string>::pure(123)
        ->flatScan<int>(0, [](auto, auto) {
            return Observable<int, std::string>::raiseError("broke");
        })
        ->last()
        .failed()
        .run(sched);
    
    sched->run_ready_tasks();
    
    auto result = fiber->await();
    EXPECT_EQ(result, "broke");
}

TEST(ObservableFlatScan, Stop) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::sequence(
            1,2,3,4,5
        )
        ->flatScan<int>(0, [](auto acc, auto value) {
            return Observable<int>::pure(acc + value);
        })
        ->take(1)
        .run(sched);
    
    sched->run_ready_tasks();

    auto result = fiber->await();

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 1);
}

TEST(ObservableFlatScan, StopInner) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::sequence(
            1, 2, 3
        )
        ->flatScan<int>(0, [](auto acc, auto value) {
            return Observable<int>::sequence(
                acc * value,
                (acc * value) + 1,
                (acc * value) + 2
            );
        })
        ->take(2)
        .run(sched);
    
    sched->run_ready_tasks();

    auto result = fiber->await();

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 1);
}
