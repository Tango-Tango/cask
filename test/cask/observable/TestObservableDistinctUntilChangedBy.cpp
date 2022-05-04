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

using SomeData = std::tuple<std::string,int>;

TEST(ObservableDistinctUntilChangedBy, Empty) {
    auto result = Observable<SomeData>::empty()
        ->distinctUntilChangedBy([](auto left, auto right) { return std::get<0>(left) == std::get<0>(right); })
        ->take(10)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 0);
}

TEST(ObservableDistinctUntilChangedBy, Error) {
    auto error = Observable<SomeData, std::string>::raiseError("broke")
        ->distinctUntilChangedBy([](auto left, auto right) { return std::get<0>(left) == std::get<0>(right); })
        ->take(10)
        .failed()
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(error, "broke");
}

TEST(ObservableDistinctUntilChangedBy, Cancel) {
    auto fiber = Observable<SomeData, std::string>::never()
        ->distinctUntilChangedBy([](auto left, auto right) { return std::get<0>(left) == std::get<0>(right); })
        ->take(10)
        .run(Scheduler::global());

    fiber->cancel();

    try {
        fiber->await();
        FAIL();
    } catch(std::runtime_error&) {}
}

TEST(ObservableDistinctUntilChangedBy, SequentialNumbers) {
    auto result = Observable<SomeData>::fromVector({
            SomeData("0", 0), SomeData("1", 0), SomeData("2", 0)
        })
        ->distinctUntilChangedBy([](auto left, auto right) { return std::get<0>(left) == std::get<0>(right); })
        ->take(10)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(std::get<0>(result[0]), "0");
    EXPECT_EQ(std::get<0>(result[1]), "1");
    EXPECT_EQ(std::get<0>(result[2]), "2");
}

TEST(ObservableDistinctUntilChangedBy, SupressDuplicates) {
    auto result = Observable<SomeData>::fromVector({
            SomeData("0", 1), SomeData("0", 2), SomeData("0", 3)
        })
        ->distinctUntilChangedBy([](auto left, auto right) { return std::get<0>(left) == std::get<0>(right); })
        ->take(10)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(std::get<0>(result[0]), "0");
    EXPECT_EQ(std::get<1>(result[0]), 1);
}
