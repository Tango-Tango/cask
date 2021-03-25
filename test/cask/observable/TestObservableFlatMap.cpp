//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"

using cask::Observable;
using cask::ObservableRef;
using cask::Scheduler;
using cask::Task;

TEST(ObservableFlatMap, Pure) {
    auto sched = Scheduler::global();
    auto result = Observable<int>::pure(123)
        ->flatMap<float>([](auto value) {
            return Observable<float>::pure(value * 1.5);
        })
        ->last()
        .run(sched)
        ->await();

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 184.5);
}

TEST(ObservableFlatMap, UpstreamError) {
    auto sched = Scheduler::global();
    auto result = Observable<int,std::string>::raiseError("broke")
        ->flatMap<float>([](auto value) {
            return Observable<float,std::string>::pure(value * 1.5);
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "broke");
}

TEST(ObservableFlatMap, ProducesError) {
    auto sched = Scheduler::global();
    auto result = Observable<int,std::string>::pure(123)
        ->flatMap<float>([](auto) {
            return Observable<float,std::string>::raiseError("broke");
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "broke");
}

TEST(ObservableFlatMap, ErrorStopsInfiniteUpstream) {
    auto sched = Scheduler::global();
    int counter = 0;
    auto result = Observable<int,std::string>::repeatTask(Task<int,std::string>::pure(123))
        ->flatMap<float>([&counter](auto) {
            counter++;
            return Observable<float,std::string>::raiseError("broke");
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "broke");
    EXPECT_EQ(counter, 1);
}

TEST(ObservableFlatMap, StopsUpstreamOnDownstreamComplete) {
    auto sched = Scheduler::global();
    int counter = 0;
    auto result = Observable<int,std::string>::repeatTask(Task<int,std::string>::pure(123))
        ->flatMap<float>([&counter](auto) {
            counter++;
            return Observable<float,std::string>::pure(123 * 1.5f);
        })
        ->take(10)
        .run(sched)
        ->await();

    EXPECT_EQ(result.size(), 10);
    EXPECT_EQ(counter, 10);

}
