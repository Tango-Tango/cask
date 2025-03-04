//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/None.hpp"
#include "cask/Scheduler.hpp"
#include "SchedulerTestBench.hpp"

#include <optional>

using cask::Observable;
using cask::ObservableRef;
using cask::Task;
using cask::None;
using cask::Scheduler;
using cask::scheduler::SingleThreadScheduler;
using cask::scheduler::WorkStealingScheduler;

INSTANTIATE_SCHEDULER_TEST_BENCH_SUITE(ObservableTest);

TEST_P(ObservableTest, Empty) {
    auto result = Observable<int>::empty()
        ->last()
        .run(sched)
        ->await();

    EXPECT_FALSE(result.has_value());
}

TEST_P(ObservableTest, RaiseError) {
    auto result = Observable<int, std::string>::raiseError("broke")
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "broke");
}

TEST_P(ObservableTest, Eval) {
    auto result = Observable<int, std::string>::eval([]() {
            return 123;
        })
        ->last()
        .run(sched)
        ->await();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123); // NOLINT(bugprone-unchecked-optional-access)
}

TEST_P(ObservableTest, EvalThrows) {
    auto result = Observable<int, std::string>::eval([]() -> int {
            throw std::string("error");
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "error");
}

TEST_P(ObservableTest, Defer) {
    auto result = Observable<int, std::string>::defer([]() {
            return Observable<int, std::string>::pure(123);
        })
        ->last()
        .run(sched)
        ->await();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123); // NOLINT(bugprone-unchecked-optional-access)
}

TEST_P(ObservableTest, DeferThrows) {
    auto result = Observable<int, std::string>::defer([]() {
            throw std::string("broke");
            return Observable<int, std::string>::pure(123);
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "broke");
}

TEST_P(ObservableTest, DeferRaisesError) {
    auto result = Observable<int, std::string>::defer([]() {
            return Observable<int, std::string>::raiseError("broke");
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "broke");
}

TEST_P(ObservableTest, DeferTask) {
    auto result = Observable<int, std::string>::deferTask([]() {
            return Task<int, std::string>::pure(123);
        })
        ->last()
        .run(sched)
        ->await();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);  // NOLINT(bugprone-unchecked-optional-access)
}

TEST_P(ObservableTest, DeferTaskThrows) {
    auto result = Observable<int, std::string>::deferTask([]() {
            throw std::string("broke");
            return Task<int, std::string>::pure(123);
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "broke");
}

TEST_P(ObservableTest, DeferTaskRaisesError) {
    auto result = Observable<int, std::string>::deferTask([]() {
            return Task<int, std::string>::raiseError("broke");
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "broke");
}

TEST_P(ObservableTest, FromVector) {
    std::vector<int> things = {1,2,3};

    auto result = Observable<int>::fromVector(things)
        ->take(3)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 3);
}

TEST_P(ObservableTest, FromVectorEnds) {
    std::vector<int> things = {1,2,3};

    auto result = Observable<int>::fromVector(things)
        ->take(10)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 3);
}

TEST_P(ObservableTest, FromVectorEmpty) {
    std::vector<int> things;

    auto result = Observable<int>::fromVector(things)
        ->take(10)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 0);
}

TEST_P(ObservableTest, CompletedEmpty) {
    auto result = Observable<int>::empty()
        ->completed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, None());
}

TEST_P(ObservableTest, CompletedNonEmpty) {
    int counter = 0;
    auto result = Observable<int>::repeatTask(
            Task<int>::eval([&counter]() {
                counter++;
                return counter;
            })
        )
        ->takeWhile([](auto i) { return i < 3; })
        ->completed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, None());
    EXPECT_EQ(counter, 3);
}

