//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/None.hpp"

using cask::Observable;
using cask::Scheduler;

TEST(ObservableLast, Pure) {
    auto result = Observable<int>::pure(123)
        ->last()
        .run(Scheduler::global())
        ->await();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);
}

TEST(ObservableLast, Error) {
    auto result = Observable<int,float>::raiseError(1.23)
        ->last()
        .failed()
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(result, 1.23f);
}

TEST(ObservableLast, Empty) {
    auto result = Observable<int>::empty()
        ->last()
        .run(Scheduler::global())
        ->await();

    EXPECT_FALSE(result.has_value());
}

TEST(ObservableLast, FiniteValues) {
    std::vector<int> values = {1,2,3,4,5};

    auto result = Observable<int>::fromVector(values)
        ->last()
        .run(Scheduler::global())
        ->await();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 5);
}

TEST(ObservableLast, MergedFiniteValues) {
    std::vector<int> first_values = {1,2,3,4,5};
    std::vector<int> second_values = {6,7,8,9,10};
    std::vector<int> third_values = {11,12,13,14,15};
    std::vector<std::vector<int>> all_values = {first_values, second_values, third_values};

    auto result = Observable<std::vector<int>>::fromVector(all_values)
        ->template flatMap<int>([](auto vector) { return Observable<int>::fromVector(vector); })
        ->last()
        .run(Scheduler::global())
        ->await();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 15);
}
