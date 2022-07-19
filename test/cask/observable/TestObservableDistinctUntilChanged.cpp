//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/None.hpp"

using cask::BufferRef;
using cask::Observable;
using cask::Scheduler;
using cask::Task;

TEST(ObservableDistinctUntilChanged, Empty) {
    auto result = Observable<int>::empty()
        ->distinctUntilChanged()
        ->take(10)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 0);
}

TEST(ObservableDistinctUntilChanged, Error) {
    auto error = Observable<int, std::string>::raiseError("broke")
        ->distinctUntilChanged()
        ->take(10)
        .failed()
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(error, "broke");
}

TEST(ObservableDistinctUntilChanged, Cancel) {
    auto fiber = Observable<int, std::string>::never()
        ->distinctUntilChanged()
        ->take(10)
        .run(Scheduler::global());

    fiber->cancel();

    try {
        fiber->await();
        FAIL();
    } catch(std::runtime_error&) {}
}

TEST(ObservableDistinctUntilChanged, SequentialNumbers) {
    auto result = Observable<int>::sequence(0, 1, 2, 3, 4)
        ->distinctUntilChanged()
        ->take(10)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 1);
    EXPECT_EQ(result[2], 2);
    EXPECT_EQ(result[3], 3);
    EXPECT_EQ(result[4], 4);
}

TEST(ObservableDistinctUntilChanged, SuppressesAllDuplicates) {
    auto result = Observable<int>::sequence(1, 1, 1, 1, 1)
        ->distinctUntilChanged()
        ->take(10)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 1);
}

TEST(ObservableDistinctUntilChanged, SuppressesRepeatedDuplicates) {
    auto result = Observable<int>::sequence(0, 0, 1, 1, 2, 2, 3, 3, 4, 4)
        ->distinctUntilChanged()
        ->take(10)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 1);
    EXPECT_EQ(result[2], 2);
    EXPECT_EQ(result[3], 3);
    EXPECT_EQ(result[4], 4);
}

TEST(ObservableDistinctUntilChanged, DoesntSuppressToggling) {
    auto result = Observable<int>::sequence(0, 1, 0, 1, 1)
        ->distinctUntilChanged()
        ->take(10)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 4);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 1);
    EXPECT_EQ(result[2], 0);
    EXPECT_EQ(result[3], 1);
}


TEST(ObservableDistinctUntilChanged, DoesntSuppressTogglingRepeats) {
    auto result = Observable<int>::sequence(0, 0, 1, 1, 0, 0, 1, 1)
        ->distinctUntilChanged()
        ->take(10)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 4);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 1);
    EXPECT_EQ(result[2], 0);
    EXPECT_EQ(result[3], 1);
}
