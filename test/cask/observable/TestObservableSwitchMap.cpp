//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include <thread>
#include "gtest/gtest.h"
#include "gtest/trompeloeil.hpp"
#include "cask/Observable.hpp"

using cask::Observable;
using cask::ObservableRef;
using cask::Observer;
using cask::Scheduler;
using cask::Task;
using cask::None;
using cask::Ack;
using cask::observable::SwitchMapObserver;

class MockSwitchMapDownstreamObserver : public trompeloeil::mock_interface<Observer<float,std::string>> {
public:
    IMPLEMENT_MOCK1(onNext);
    IMPLEMENT_MOCK1(onError);
    IMPLEMENT_MOCK0(onComplete);
    IMPLEMENT_MOCK0(onCancel);
};

void awaitIdle() {
    const static std::chrono::milliseconds sleep_time(1);

    int num_retries = 60000;
    while(num_retries > 0) {
        if(Scheduler::global()->isIdle()) {
            return;
        } else {
            std::this_thread::sleep_for(sleep_time);
            num_retries--;
        }
    }

    FAIL() << "Expected scheduler to return to idle within 60 seconds.";
}

TEST(ObservableSwitchMap, Empty) {
    auto sched = Scheduler::global();
    auto result = Observable<int>::empty()
        ->switchMap<float>([](auto value) {
            return Observable<float>::pure(value * 1.5);
        })
        ->last()
        .run(sched)
        ->await();

    EXPECT_TRUE(!result.has_value());
    awaitIdle();
}

TEST(ObservableSwitchMap, Pure) {
    auto sched = Scheduler::global();
    auto result = Observable<int>::pure(123)
        ->switchMap<float>([](auto value) {
            return Observable<float>::pure(value * 1.5);
        })
        ->last()
        .run(sched)
        ->await();

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 184.5);
    awaitIdle();
}

TEST(ObservableSwitchMap, UpstreamError) {
    auto sched = Scheduler::global();
    auto result = Observable<int,std::string>::raiseError("broke")
        ->switchMap<float>([](auto value) {
            return Observable<float,std::string>::pure(value * 1.5);
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "broke");
    awaitIdle();
}

TEST(ObservableSwitchMap, ProducesError) {
    auto sched = Scheduler::global();
    auto result = Observable<int,std::string>::pure(123)
        ->switchMap<float>([](auto) {
            return Observable<float,std::string>::raiseError("broke");
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "broke");
    awaitIdle();
}

TEST(ObservableSwitchMap, ErrorStopsInfiniteUpstream) {
    auto sched = Scheduler::global();
    int counter = 0;
    auto result = Observable<int,std::string>::repeatTask(Task<int,std::string>::pure(123).delay(1))
        ->switchMap<float>([&counter](auto) {
            counter++;
            return Observable<float,std::string>::raiseError("broke");
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "broke");
    EXPECT_EQ(counter, 1);
    awaitIdle();
}

TEST(ObservableSwitchMap, CancelStopsInfiniteUpstream) {
    auto sched = Scheduler::global();
    auto deferred = Observable<int,std::string>::repeatTask(
            Task<int,std::string>::pure(123).delay(10)
        )
        ->switchMap<float>([](auto) {
            return Observable<float,std::string>::pure(1.23f);
        })
        ->completed()
        .run(sched);

    deferred->cancel();

    try {
        deferred->await();
        FAIL() << "Expected method to throw";
    } catch(std::runtime_error&) {}

    awaitIdle();
}

TEST(ObservableSwitchMap, StopsUpstreamOnDownstreamComplete) {
    auto sched = Scheduler::global();
    int counter = 0;
    auto result = Observable<int,std::string>::repeatTask(Task<int,std::string>::pure(123).delay(1))
        ->switchMap<float>([&counter](auto) {
            counter++;
            return Observable<float,std::string>::pure(123 * 1.5f);
        })
        ->take(10)
        .run(sched)
        ->await();

    EXPECT_EQ(result.size(), 10);
    EXPECT_GE(counter, 10);
    EXPECT_LE(counter, 11);
    awaitIdle();
}
