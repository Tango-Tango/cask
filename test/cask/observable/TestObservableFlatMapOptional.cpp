//          Copyright Tango Tango, Inc. 2022.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::Observable;
using cask::Task;
using cask::scheduler::BenchScheduler;

TEST(ObservableFlatMapOptional, Value) {
    auto sched = std::make_shared<BenchScheduler>();

    auto fiber = Observable<int>::pure(1)
        ->flatMapOptional<float>([](auto value) {
            return std::optional<float>(value * 1.5);
        })
        ->last()
        .run(sched);

    sched->run_ready_tasks();
    auto result = fiber->await();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1.5); // NOLINT(bugprone-unchecked-optional-access)
}

TEST(ObservableFlatMapOptional, UpstreamEmpty) {
    auto sched = std::make_shared<BenchScheduler>();

    auto fiber = Observable<int>::empty()
        ->flatMapOptional<float>([](auto value) {
            return std::optional<float>(value * 1.5);
        })
        ->last()
        .run(sched);

    sched->run_ready_tasks();
    auto result = fiber->await();
    ASSERT_FALSE(result.has_value());
}

TEST(ObservableFlatMapOptional, DowsntreamEmpty) {
    auto sched = std::make_shared<BenchScheduler>();

    auto fiber = Observable<int>::pure(1)
        ->flatMapOptional<float>([](auto) {
            return std::optional<float>();
        })
        ->last()
        .run(sched);

    sched->run_ready_tasks();
    auto result = fiber->await();
    ASSERT_FALSE(result.has_value());
}
