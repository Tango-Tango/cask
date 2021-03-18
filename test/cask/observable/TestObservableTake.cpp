//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/None.hpp"

using cask::Observable;
using cask::Scheduler;

TEST(ObservableTake, PureTakeNothing) {
    auto result = Observable<int>::pure(123)
        ->take(0)
        .run(Scheduler::global())
        ->await();

    EXPECT_TRUE(result.empty());
}

TEST(ObservableTake, PureTakeOne) {
    auto result = Observable<int>::pure(123)
        ->take(1)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 123);
}

TEST(ObservableTake, PureTakeMultiple) {
    auto result = Observable<int>::pure(123)
        ->take(2)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 123);
}

TEST(ObservableTake, VectorTakeNothing) {
    std::vector<int> values = {1,2,3,4,5};
    auto result = Observable<int>::fromVector(values)
        ->take(0)
        .run(Scheduler::global())
        ->await();

    EXPECT_TRUE(result.empty());
}

TEST(ObservableTake, VectorTakeOne) {
    std::vector<int> values = {1,2,3,4,5};
    auto result = Observable<int>::fromVector(values)
        ->take(1)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 1);
}

TEST(ObservableTake, VectorTakeMulti) {
    std::vector<int> values = {1,2,3,4,5};
    auto result = Observable<int>::fromVector(values)
        ->take(3)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 3);
}


TEST(ObservableTake, VectorTakeAll) {
    std::vector<int> values = {1,2,3,4,5};
    auto result = Observable<int>::fromVector(values)
        ->take(5)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 3);
    EXPECT_EQ(result[3], 4);
    EXPECT_EQ(result[4], 5);
}

TEST(ObservableTake, VectorTakeMoreThanAll) {
    std::vector<int> values = {1,2,3,4,5};
    auto result = Observable<int>::fromVector(values)
        ->take(6)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 3);
    EXPECT_EQ(result[3], 4);
    EXPECT_EQ(result[4], 5);
}

TEST(Observable, VectorTakeMergedNone) {
    std::vector<int> first_values = {1,2,3,4,5};
    std::vector<int> second_values = {6,7,8,9,10};
    std::vector<int> third_values = {11,12,13,14,15};
    std::vector<std::vector<int>> all_values = {first_values, second_values, third_values};

    auto result = Observable<std::vector<int>>::fromVector(all_values)
        ->template flatMap<int>([](auto vector) { return Observable<int>::fromVector(vector); })
        ->take(0)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 0);
}

TEST(Observable, VectorTakeMergedMulti) {
    std::vector<int> first_values = {1,2,3,4,5};
    std::vector<int> second_values = {6,7,8,9,10};
    std::vector<int> third_values = {11,12,13,14,15};
    std::vector<std::vector<int>> all_values = {first_values, second_values, third_values};

    auto result = Observable<std::vector<int>>::fromVector(all_values)
        ->template flatMap<int>([](auto vector) { return Observable<int>::fromVector(vector); })
        ->take(6)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 6);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 3);
    EXPECT_EQ(result[3], 4);
    EXPECT_EQ(result[4], 5);
    EXPECT_EQ(result[5], 6);
}

TEST(Observable, VectorTakeMergedAll) {
    std::vector<int> first_values = {1,2,3,4,5};
    std::vector<int> second_values = {6,7,8,9,10};
    std::vector<int> third_values = {11,12,13,14,15};
    std::vector<std::vector<int>> all_values = {first_values, second_values, third_values};

    auto result = Observable<std::vector<int>>::fromVector(all_values)
        ->template flatMap<int>([](auto vector) { return Observable<int>::fromVector(vector); })
        ->take(15)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 15);
    for(unsigned int i = 0; i < result.size(); i++) {
        EXPECT_EQ(result[i], i+1);
    }
}

TEST(Observable, VectorTakeMergedMoreThanAll) {
    std::vector<int> first_values = {1,2,3,4,5};
    std::vector<int> second_values = {6,7,8,9,10};
    std::vector<int> third_values = {11,12,13,14,15};
    std::vector<std::vector<int>> all_values = {first_values, second_values, third_values};

    auto result = Observable<std::vector<int>>::fromVector(all_values)
        ->template flatMap<int>([](auto vector) { return Observable<int>::fromVector(vector); })
        ->take(16)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 15);
    for(unsigned int i = 0; i < result.size(); i++) {
        EXPECT_EQ(result[i], i+1);
    }
}
