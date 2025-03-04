//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/None.hpp"

using cask::Observable;
using cask::Scheduler;
using cask::Task;

TEST(ObservableMap, Empty) {
    auto sched = Scheduler::global();
    auto result = Observable<int>::empty()
        ->map<float>([](auto value) { return value * 1.5; })
        ->last()
        .run(sched)
        ->await();

    EXPECT_FALSE(result.has_value());
}

TEST(ObservableMap, Value) {
    auto sched = Scheduler::global();
    auto result = Observable<int>::pure(123)
        ->map<float>([](auto value) { return value * 1.5; })
        ->last()
        .run(sched)
        ->await();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 184.5); // NOLINT(bugprone-unchecked-optional-access)
}

TEST(ObservableMap, Error) {
    auto sched = Scheduler::global();
    auto result = Observable<int,std::string>::raiseError("broke")
        ->map<float>([](auto value) { return value * 1.5; })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "broke");
}
