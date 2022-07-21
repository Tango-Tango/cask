//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)


#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/scheduler/SingleThreadScheduler.hpp"

using cask::Observable;
using cask::scheduler::SingleThreadScheduler;

TEST(ObservableMergeMapTest,Empty) {
    auto sched = std::make_shared<SingleThreadScheduler>();
    auto fiber = Observable<int,std::string>::empty()
        ->template mergeMap<int>([](auto value) {
            return Observable<int,std::string>::pure(value * 2);
        })
        ->last()
        .run(sched);

    auto result = fiber->await();
    EXPECT_FALSE(result.has_value());
}

TEST(ObservableMergeMapTest,PureValues) {
    auto sched = std::make_shared<SingleThreadScheduler>();
    auto fiber = Observable<int,std::string>::sequence(1, 2, 3, 4, 5, 6)
        ->template mergeMap<int>([](auto value) {
            return Observable<int,std::string>::pure(value * 2);
        })
        ->last()
        .run(sched);

    auto result = fiber->await();
    EXPECT_TRUE(result.has_value());
}