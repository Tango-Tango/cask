//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)


#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/scheduler/SingleThreadScheduler.hpp"

using cask::Observable;
using cask::scheduler::SingleThreadScheduler;

TEST(ObservableMerge,Empty) {
    auto sched = std::make_shared<SingleThreadScheduler>();
    auto fiber = Observable<int,std::string>::empty()
        ->merge(Observable<int,std::string>::empty())
        ->last()
        .run(sched);

    auto result = fiber->await();
    EXPECT_FALSE(result.has_value());
}

TEST(ObservableMerge,LeftValue) {
    auto sched = std::make_shared<SingleThreadScheduler>();
    auto fiber = Observable<int,std::string>::pure(123)
        ->merge(Observable<int,std::string>::empty())
        ->last()
        .run(sched);

    auto result = fiber->await();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);
}

TEST(ObservableMerge,RightValue) {
    auto sched = std::make_shared<SingleThreadScheduler>();
    auto fiber = Observable<int,std::string>::empty()
        ->merge(Observable<int,std::string>::pure(123))
        ->last()
        .run(sched);

    auto result = fiber->await();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);
}

TEST(ObservableMerge,LeftError) {
    auto sched = std::make_shared<SingleThreadScheduler>();
    auto fiber = Observable<int,std::string>::raiseError("broke")
        ->merge(Observable<int,std::string>::empty())
        ->last()
        .failed()
        .run(sched);

    auto result = fiber->await();
    EXPECT_EQ(result, "broke");
}

TEST(ObservableMerge,RightError) {
    auto sched = std::make_shared<SingleThreadScheduler>();
    auto fiber = Observable<int,std::string>::empty()
        ->merge(Observable<int,std::string>::raiseError("broke"))
        ->last()
        .failed()
        .run(sched);

    auto result = fiber->await();
    EXPECT_EQ(result, "broke");
}
