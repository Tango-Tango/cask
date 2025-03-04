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

// NOLINTBEGIN(bugprone-unchecked-optional-access)

TEST(ObservableMapError, Empty) {
    auto sched = Scheduler::global();
    auto result = Observable<int, std::string>::empty()
        ->mapError<std::string>([](auto error) {
            std::string copy(error);
            std::reverse(copy.begin(), copy.end());
            return copy;
        })
        ->last()
        .run(sched)
        ->await();

    EXPECT_FALSE(result.has_value());
}

TEST(ObservableMapError, Value) {
    auto sched = Scheduler::global();
    auto result = Observable<int, std::string>::pure(123)
        ->mapError<std::string>([](auto error) {
            std::string copy(error);
            std::reverse(copy.begin(), copy.end());
            return copy;
        })
        ->last()
        .run(sched)
        ->await();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);
}

TEST(ObservableMapError, ErrorSameType) {
    auto sched = Scheduler::global();
    auto result = Observable<int, std::string>::raiseError("broke")
        ->mapError<std::string>([](auto error) {
            std::string copy(error);
            std::reverse(copy.begin(), copy.end());
            return copy;
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "ekorb");
}

TEST(ObservableMapError, ErrorDifferentType) {
    auto sched = Scheduler::global();

    auto result = Observable<int,int>::raiseError(123)
        ->mapError<std::string>([](auto err) {
            return std::to_string(err);
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "123");
}

TEST(ObservableMapError, ErrorUpstream) {
    auto counter = 0;
    auto sched = Scheduler::global();

    auto result = Observable<int,int>::repeatTask(Task<int,int>::pure(123))
        ->flatMap<int>([](auto) {
            return Observable<int,int>::raiseError(456);
        })
        ->mapError<std::string>([&counter](auto err) {
            counter++;
            return std::to_string(err);
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "456");
    EXPECT_EQ(counter, 1);
}

TEST(ObservableMapError, ErrorDownstream) {
    auto counter = 0;
    auto sched = Scheduler::global();

    auto result = Observable<int,int>::repeatTask(Task<int,int>::pure(123))
        ->mapError<std::string>([](auto err) {
            return std::to_string(err);
        })
        ->flatMap<int>([&counter](auto) {
            counter++;
            return Observable<int,std::string>::raiseError("456");
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "456");
    EXPECT_EQ(counter, 1);
}

// NOLINTEND(bugprone-unchecked-optional-access)
