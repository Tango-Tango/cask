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
using cask::None;

TEST(ObservableTakeWhileInclusive, Error) {
    auto result = Observable<int,float>::raiseError(1.23)
        ->takeWhileInclusive([](auto) {
            return true;
        })
        ->last()
        .failed()
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(result, 1.23f);
}

TEST(ObservableTakeWhileInclusive, Empty) {
    auto result = Observable<int>::empty()
        ->takeWhileInclusive([](auto) {
            return true;
        })
        ->last()
        .run(Scheduler::global())
        ->await();

    EXPECT_FALSE(result.has_value());
}


TEST(ObservableTakeWhileInclusive, Infinite) {
    int counter = 0;
    auto result = Observable<int>::repeatTask(Task<int>::eval([&counter] {
            counter++;
            return counter;
        }))
        ->takeWhileInclusive([](auto value) {
            return value < 10;
        })
        ->take(100)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 10);
    for(unsigned int i = 0; i < result.size(); i++) {
        EXPECT_EQ(result[i], i+1);
    }
}

TEST(ObservableTakeWhileInclusive, Finite) {
    auto result = Observable<int>::pure(123)
        ->takeWhileInclusive([](auto) {
            return true;
        })
        ->take(100)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 123);
}

TEST(ObservableTakeWhileInclusive, RunsCompletionTask) {
    auto shutdown_counter = 0;

    auto result = Observable<int>::pure(123)
        ->takeWhileInclusive([](auto) {
            return false;
        })
        ->guarantee(Task<None,None>::eval([&shutdown_counter] {
            shutdown_counter++;
            return None();

        }))
        ->take(100)
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(shutdown_counter, 1);
}

TEST(ObservableTakeWhileInclusive, RunsErrorTask) {
    auto shutdown_counter = 0;

    auto result = Observable<int,std::string>::raiseError("broke")
        ->takeWhileInclusive([](auto) {
            return false;
        })
        ->guarantee(Task<None,None>::eval([&shutdown_counter] {
            shutdown_counter++;
            return None();

        }))
        ->take(100)
        .failed()
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(result, "broke");
    EXPECT_EQ(shutdown_counter, 1);
}
