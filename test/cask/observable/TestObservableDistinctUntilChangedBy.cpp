//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/None.hpp"
#include "cask/scheduler/WorkStealingScheduler.hpp"

using cask::BufferRef;
using cask::Observable;
using cask::Scheduler;
using cask::Task;
using cask::scheduler::SingleThreadScheduler;
using cask::scheduler::WorkStealingScheduler;
using SomeData = std::tuple<std::string,int>;

class ObservableDistinctUntilChangedByTest : public ::testing::TestWithParam<std::shared_ptr<Scheduler>> {
protected:

    void SetUp() override {
        sched = GetParam();
    }

    std::shared_ptr<Scheduler> sched;
};

TEST_P(ObservableDistinctUntilChangedByTest, Empty) {
    auto result = Observable<SomeData>::empty()
        ->distinctUntilChangedBy([](auto left, auto right) { return std::get<0>(left) == std::get<0>(right); })
        ->take(10)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 0);
}

TEST_P(ObservableDistinctUntilChangedByTest, Error) {
    auto error = Observable<SomeData, std::string>::raiseError("broke")
        ->distinctUntilChangedBy([](auto left, auto right) { return std::get<0>(left) == std::get<0>(right); })
        ->take(10)
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(error, "broke");
}

TEST_P(ObservableDistinctUntilChangedByTest, Cancel) {
    auto fiber = Observable<SomeData, std::string>::never()
        ->distinctUntilChangedBy([](auto left, auto right) { return std::get<0>(left) == std::get<0>(right); })
        ->take(10)
        .run(sched);

    fiber->cancel();

    try {
        fiber->await();
        FAIL();
    } catch(std::runtime_error&) {}
}

TEST_P(ObservableDistinctUntilChangedByTest, SequentialNumbers) {
    auto result = Observable<SomeData>::sequence(
            SomeData("0", 0), SomeData("1", 0), SomeData("2", 0)
        )
        ->distinctUntilChangedBy([](auto left, auto right) { return std::get<0>(left) == std::get<0>(right); })
        ->take(10)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(std::get<0>(result[0]), "0");
    EXPECT_EQ(std::get<0>(result[1]), "1");
    EXPECT_EQ(std::get<0>(result[2]), "2");
}

TEST_P(ObservableDistinctUntilChangedByTest, SupressDuplicates) {
    auto result = Observable<SomeData>::sequence(
            SomeData("0", 1), SomeData("0", 2), SomeData("0", 3)
        )
        ->distinctUntilChangedBy([](auto left, auto right) { return std::get<0>(left) == std::get<0>(right); })
        ->take(10)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(std::get<0>(result[0]), "0");
    EXPECT_EQ(std::get<1>(result[0]), 1);
}

INSTANTIATE_TEST_SUITE_P(ObservableDistinctUntilChangedByTest, ObservableDistinctUntilChangedByTest,
    ::testing::Values(
        std::make_shared<SingleThreadScheduler>(),
        std::make_shared<WorkStealingScheduler>(1),
        std::make_shared<WorkStealingScheduler>(2),
        std::make_shared<WorkStealingScheduler>(4),
        std::make_shared<WorkStealingScheduler>(8)
    ),
    [](const ::testing::TestParamInfo<ObservableDistinctUntilChangedByTest::ParamType>& info) {
        return info.param->toString();
    }
);
