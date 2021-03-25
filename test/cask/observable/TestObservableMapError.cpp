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

TEST(ObservableMapError, SameType) {
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

TEST(ObservableMapError, DifferentType) {
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

TEST(ObservableMapError, UpstreamError) {
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

TEST(ObservableMapError, DownstreamError) {
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
