//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/None.hpp"

using cask::Scheduler;
using cask::Observable;
using cask::Task;

TEST(Observable, PureLast) {
    auto sched = Scheduler::global();
    auto result = Observable<int>::pure(123)
        ->last()
        .run(sched)
        ->await();

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);
}

TEST(Observable, PureMap) {
    auto sched = Scheduler::global();
    auto result = Observable<int>::pure(123)
        ->map<float>([](auto value) { return value * 1.5; })
        ->last()
        .run(sched)
        ->await();

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 184.5);
}

TEST(Observable, PureMapTask) {
    auto sched = Scheduler::global();
    auto result = Observable<int>::pure(123)
        ->mapTask<float>([](auto value) {
            return Task<float>::pure(value * 1.5);
        })
        ->last()
        .run(sched)
        ->await();

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 184.5);
}

TEST(Observable, PureFlatMap) {
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

TEST(Observable, PureTake) {
    auto sched = Scheduler::global();
    auto result = Observable<int>::pure(123)
        ->take(10)
        .run(sched)
        ->await();

    EXPECT_EQ(result.size(), 1);
}

TEST(Observable, RepeatTaskTake) {
    auto sched = Scheduler::global();
    auto result = Observable<int>::repeatTask(cask::Task<int>::pure(123))
        ->take(10)
        .run(sched)
        ->await();

    EXPECT_EQ(result.size(), 10);
}

TEST(Observable, Empty) {
    auto sched = Scheduler::global();
    auto result = Observable<int>::empty()
        ->last()
        .run(sched)
        ->await();

    EXPECT_FALSE(result.has_value());
}

TEST(Observable, RaiseError) {
    auto sched = Scheduler::global();
    auto result = Observable<int, std::string>::raiseError("broke")
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "broke");
}

TEST(Observable, Eval) {
    auto sched = Scheduler::global();
    auto result = Observable<int, std::string>::eval([]() {
            return 123;
        })
        ->last()
        .run(sched)
        ->await();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);
}

TEST(Observable, EvalThrows) {
    auto sched = Scheduler::global();
    auto result = Observable<int, std::string>::eval([]() -> int {
            throw std::string("error");
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "error");
}

TEST(Observable, Defer) {
    auto sched = Scheduler::global();
    auto result = Observable<int, std::string>::defer([]() {
            return Observable<int, std::string>::pure(123);
        })
        ->last()
        .run(sched)
        ->await();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);
}

TEST(Observable, DeferThrows) {
    auto sched = Scheduler::global();
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

TEST(Observable, DeferRaisesError) {
    auto sched = Scheduler::global();
    auto result = Observable<int, std::string>::defer([]() {
            return Observable<int, std::string>::raiseError("broke");
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "broke");
}

TEST(Observable, DeferTask) {
    auto sched = Scheduler::global();
    auto result = Observable<int, std::string>::deferTask([]() {
            return Task<int, std::string>::pure(123);
        })
        ->last()
        .run(sched)
        ->await();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);
}

TEST(Observable, DeferTaskThrows) {
    auto sched = Scheduler::global();
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

TEST(Observable, DeferTaskRaisesError) {
    auto sched = Scheduler::global();
    auto result = Observable<int, std::string>::deferTask([]() {
            return Task<int, std::string>::raiseError("broke");
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "broke");
}

TEST(Observable, TakeWhile) {
    int counter = 0;
    auto result = Observable<int>::repeatTask(Task<int>::eval([&counter] {
            counter++;
            return counter;
        }))
        ->takeWhile([](auto value) {
            return value <= 10;
        })
        ->last()
        .run(Scheduler::global())
        ->await();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 10);
}