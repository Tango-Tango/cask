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

TEST(ObservableTakeWhile, Error) {
    auto result = Observable<int,float>::raiseError(1.23)
        ->takeWhile([](auto) {
            return true;
        })
        ->last()
        .failed()
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(result, 1.23f);
}

TEST(ObservableTakeWhile, Empty) {
    auto result = Observable<int>::empty()
        ->takeWhile([](auto) {
            return true;
        })
        ->last()
        .run(Scheduler::global())
        ->await();

    EXPECT_FALSE(result.has_value());
}

TEST(ObservableTakeWhile, Infinite) {
    int counter = 0;
    auto result = Observable<int>::repeatTask(Task<int>::eval([&counter] {
            counter++;
            return counter;
        }))
        ->takeWhile([](auto value) {
            return value < 10;
        })
        ->take(100)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 9);
    for(unsigned int i = 0; i < result.size(); i++) {
        EXPECT_EQ(result[i], i+1);
    }
}

TEST(ObservableTakeWhile, Finite) {
    auto result = Observable<int>::pure(123)
        ->takeWhile([](auto) {
            return true;
        })
        ->take(100)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 123);
}
