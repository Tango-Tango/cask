//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include <thread>
#include "gtest/gtest.h"
#include "gtest/trompeloeil.hpp"
#include "cask/Observable.hpp"
#include "cask/Scheduler.hpp"
#include "cask/scheduler/SingleThreadScheduler.hpp"
#include "cask/scheduler/WorkStealingScheduler.hpp"
#include "cask/scheduler/ThreadPoolScheduler.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::Observable;
using cask::ObservableRef;
using cask::Observer;
using cask::Scheduler;
using cask::Task;
using cask::None;
using cask::Ack;
using cask::observable::SwitchMapObserver;
using cask::scheduler::BenchScheduler;
using cask::scheduler::SingleThreadScheduler;
using cask::scheduler::ThreadPoolScheduler;
using cask::scheduler::WorkStealingScheduler;

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

class ObservableSwitchMapTest : public ::testing::TestWithParam<std::shared_ptr<Scheduler>> {
protected:

    void SetUp() override {
        sched = GetParam();
    }

    std::shared_ptr<Scheduler> sched;
};


TEST_P(ObservableSwitchMapTest, Empty) {
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

TEST_P(ObservableSwitchMapTest, Pure) {
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

TEST_P(ObservableSwitchMapTest, UpstreamError) {
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

TEST_P(ObservableSwitchMapTest, ProducesError) {
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

TEST_P(ObservableSwitchMapTest, ErrorStopsInfiniteUpstream) {
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

TEST_P(ObservableSwitchMapTest, CancelStopsInfiniteUpstream) {
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

TEST_P(ObservableSwitchMapTest, StopsUpstreamOnDownstreamComplete) {
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

TEST(ObservableSwitchMap, CompletionWaitsForSubscriptionComplete) {
    auto sched = std::make_shared<BenchScheduler>();
    int counter = 0;

    auto fiber = Observable<int,std::string>::fromVector(std::vector<int>{ 1 , 2, 3 })
        ->switchMap<int>([&counter](auto value) {
            counter++;
            return Observable<int,std::string>::deferTask([value] {
                return Task<int,std::string>::pure(value * 2).delay(1);
            });
        })
        ->take(3)
        .run(sched);

    sched->run_ready_tasks();
    EXPECT_FALSE(fiber->getValue().has_value());

    sched->advance_time(1);
    sched->run_ready_tasks();
    ASSERT_TRUE(fiber->getValue().has_value());

    auto result = fiber->await();

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(counter, 3);
    EXPECT_GE(result[0], 6);
}

TEST(ObservableSwitchMap, CancelInnerObservable) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int,std::string>::fromVector(std::vector<int>{ 1 , 2, 3 })
        ->appendAll(Observable<int,std::string>::never())
        ->switchMap<int>([](auto value) {
            return Observable<int,std::string>::deferTask([value] {
                return Task<int,std::string>::pure(value * 2).delay(1);
            });
        })
        ->take(3)
        .run(sched);

    sched->run_ready_tasks();
    EXPECT_FALSE(fiber->getValue().has_value());

    fiber->cancel();
    sched->run_ready_tasks();

    EXPECT_TRUE(fiber->isCanceled());
}

INSTANTIATE_TEST_SUITE_P(ObservableSwitchMap, ObservableSwitchMapTest,
    ::testing::Values(
        std::make_shared<SingleThreadScheduler>(),
        std::make_shared<WorkStealingScheduler>(1),
        std::make_shared<WorkStealingScheduler>(2),
        std::make_shared<WorkStealingScheduler>(4),
        std::make_shared<WorkStealingScheduler>(8),
        std::make_shared<ThreadPoolScheduler>(1),
        std::make_shared<ThreadPoolScheduler>(2),
        std::make_shared<ThreadPoolScheduler>(4),
        std::make_shared<ThreadPoolScheduler>(8)
    ),
    [](const ::testing::TestParamInfo<ObservableSwitchMapTest::ParamType>& info) {
        return info.param->toString();
    }
);

