//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/None.hpp"
#include "cask/Scheduler.hpp"
#include "SchedulerTestBench.hpp"

using cask::BufferRef;
using cask::Observable;
using cask::Scheduler;
using cask::Task;

INSTANTIATE_SCHEDULER_TEST_BENCH_SUITE(ObservableDistinctUntilChangedTest);

TEST_P(ObservableDistinctUntilChangedTest, Empty) {
    auto result = Observable<int>::empty()
        ->distinctUntilChanged()
        ->take(10)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 0);
}

TEST_P(ObservableDistinctUntilChangedTest, Error) {
    auto error = Observable<int, std::string>::raiseError("broke")
        ->distinctUntilChanged()
        ->take(10)
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(error, "broke");
}

TEST_P(ObservableDistinctUntilChangedTest, Cancel) {
    auto fiber = Observable<int, std::string>::never()
        ->distinctUntilChanged()
        ->take(10)
        .run(sched);

    fiber->cancel();

    try {
        fiber->await();
        FAIL();
    } catch(std::runtime_error&) {}
}

TEST_P(ObservableDistinctUntilChangedTest, SequentialNumbers) {
    auto result = Observable<int>::sequence(0, 1, 2, 3, 4)
        ->distinctUntilChanged()
        ->take(10)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 1);
    EXPECT_EQ(result[2], 2);
    EXPECT_EQ(result[3], 3);
    EXPECT_EQ(result[4], 4);
}

TEST_P(ObservableDistinctUntilChangedTest, SuppressesAllDuplicates) {
    auto result = Observable<int>::sequence(1, 1, 1, 1, 1)
        ->distinctUntilChanged()
        ->take(10)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 1);
}

TEST_P(ObservableDistinctUntilChangedTest, SuppressesRepeatedDuplicates) {
    auto result = Observable<int>::sequence(0, 0, 1, 1, 2, 2, 3, 3, 4, 4)
        ->distinctUntilChanged()
        ->take(10)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 1);
    EXPECT_EQ(result[2], 2);
    EXPECT_EQ(result[3], 3);
    EXPECT_EQ(result[4], 4);
}

TEST_P(ObservableDistinctUntilChangedTest, DoesntSuppressToggling) {
    auto result = Observable<int>::sequence(0, 1, 0, 1, 1)
        ->distinctUntilChanged()
        ->take(10)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 4);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 1);
    EXPECT_EQ(result[2], 0);
    EXPECT_EQ(result[3], 1);
}


TEST_P(ObservableDistinctUntilChangedTest, DoesntSuppressTogglingRepeats) {
    auto result = Observable<int>::sequence(0, 0, 1, 1, 0, 0, 1, 1)
        ->distinctUntilChanged()
        ->take(10)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 4);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 1);
    EXPECT_EQ(result[2], 0);
    EXPECT_EQ(result[3], 1);
}
