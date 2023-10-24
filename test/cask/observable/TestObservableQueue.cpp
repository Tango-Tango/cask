//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/None.hpp"
#include "cask/scheduler/SingleThreadScheduler.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::Observable;
using cask::Scheduler;
using cask::Task;
using cask::observable::QueueOverflowStrategy;
using cask::scheduler::BenchScheduler;
using cask::scheduler::SingleThreadScheduler;

TEST(ObservableQueue, Empty) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::empty()
        ->queue(1)
        ->last()
        .run(sched);
    
    sched->run_ready_tasks();
    auto result = fiber->await();

    EXPECT_FALSE(result.has_value());
}

TEST(ObservableQueue, Value) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::pure(123)
        ->queue(1)
        ->last()
        .run(sched);
    
    sched->run_ready_tasks();
    auto result = fiber->await();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);
}

TEST(ObservableQueue, ValueThenError) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int,std::string>::pure(123)
        ->concat(Observable<int,std::string>::raiseError("broke"))
        ->queue(1)
        ->last()
        .failed()
        .run(sched);
    
    sched->run_ready_tasks();
    auto result = fiber->await();
    EXPECT_EQ(result, "broke");
}

TEST(ObservableQueue, Never) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::never()
        ->queue(1)
        ->last()
        .run(sched);
    
    sched->run_ready_tasks();

    EXPECT_EQ(sched->num_task_ready(), 0);
    EXPECT_EQ(sched->num_timers(), 0);
    EXPECT_FALSE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_FALSE(fiber->isCanceled());

    fiber->cancel();
    sched->run_ready_tasks();
    EXPECT_TRUE(fiber->isCanceled());
}

TEST(ObservableQueue, NeverEarlyCancel) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::never()
        ->queue(1)
        ->last()
        .asyncBoundary()
        .run(sched);
    EXPECT_EQ(sched->num_task_ready(), 1);
    EXPECT_EQ(sched->num_timers(), 0);
    EXPECT_FALSE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_FALSE(fiber->isCanceled());

    fiber->cancel();
    sched->run_ready_tasks();
    EXPECT_TRUE(fiber->isCanceled());
}

TEST(ObservableQueue, ValueThenNever) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int,std::string>::pure(123)
        ->concat(Observable<int,std::string>::never())
        ->queue(1)
        ->last()
        .run(sched);
    
    sched->run_ready_tasks();

    EXPECT_EQ(sched->num_task_ready(), 0);
    EXPECT_EQ(sched->num_timers(), 0);
    EXPECT_FALSE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_FALSE(fiber->isCanceled());

    fiber->cancel();
    sched->run_ready_tasks();
    EXPECT_TRUE(fiber->isCanceled());
}

TEST(ObservableQueue, ValuesLargerThanQueue) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::sequence(0, 1, 2, 3, 4, 5)
        ->queue(1)
        ->take(10)
        .run(sched);
    
    sched->run_ready_tasks();
    auto result = fiber->await();

    ASSERT_EQ(result.size(), 6);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 1);
    EXPECT_EQ(result[2], 2);
    EXPECT_EQ(result[3], 3);
    EXPECT_EQ(result[4], 4);
    EXPECT_EQ(result[5], 5);
}

TEST(ObservableQueue, Error) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int, std::string>::raiseError("broke")
        ->queue(1)
        ->last()
        .failed()
        .run(sched);

    sched->run_ready_tasks();
    auto result = fiber->await();

    EXPECT_EQ(result, "broke");
}

TEST(ObservableQueue, UpstreamRunsWhileDownstreamBackpressure) {
    int upstream_counter = 0;
    int downstream_counter = 0;

    auto sched = std::make_shared<BenchScheduler>();
    auto downstream_queue = cask::Queue<int,cask::None>::empty(sched, 1);
    auto fiber = Observable<int, cask::None>::sequence(0, 1, 2, 3, 4, 5)
        ->template map<int>([&upstream_counter](auto value) {
            upstream_counter++;
            return value;
        })
        ->queue(10)
        ->template mapTask<cask::None>([downstream_queue](auto value) {
            return downstream_queue->put(value);
        })
        ->template map<cask::None>([&downstream_counter](auto value) {
            downstream_counter++;
            return value;
        })
        ->completed()
        .run(sched);

    sched->run_ready_tasks();

    EXPECT_EQ(upstream_counter, 6);
    EXPECT_EQ(downstream_counter, 1);

    for (unsigned int i = 0; i < 6; i++) {
        EXPECT_EQ(downstream_queue->tryTake(), i);
        sched->run_ready_tasks();
    }

    EXPECT_EQ(upstream_counter, 6);
    EXPECT_EQ(downstream_counter, 6);

    fiber->await();
}

TEST(ObservableQueue, DownstreamStopBigQueue) {
    auto sched = std::make_shared<SingleThreadScheduler>();
    auto downstream_queue = cask::Queue<int,cask::None>::empty(sched, 1);
    auto result = Observable<int, cask::None>::sequence(0, 1, 2, 3, 4, 5)
        ->queue(10)
        ->take(2)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 1);
}

TEST(ObservableQueue, DownstreamStopSmallQueue) {
    auto sched = Scheduler::global();
    auto downstream_queue = cask::Queue<int,cask::None>::empty(sched, 1);
    auto result = Observable<int, cask::None>::sequence(0, 1, 2, 3, 4, 5)
        ->queue(1)
        ->take(2)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 1);
}

TEST(ObservableQueue, TailDropOverflowStrategy) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int, cask::None>::sequence(1, 2, 3, 4)
        ->queue(1, QueueOverflowStrategy::TailDrop)
        ->mapTask<int>([](auto&& value) {
            return Task<int, cask::None>::pure(value).delay(1);
        })
        ->last()
        .run(sched);
    
    sched->run_ready_tasks();
    sched->advance_time(1);
    sched->run_ready_tasks();
    sched->advance_time(1);
    sched->run_ready_tasks();

    auto result = fiber->await();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 2);
}

TEST(ObservableQueue, BackpressureOverflowStrategy) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int, cask::None>::sequence(1, 2, 3, 4)
        ->queue(1, QueueOverflowStrategy::Backpressure)
        ->mapTask<int>([](auto&& value) {
            return Task<int, cask::None>::pure(value).delay(1);
        })
        ->last()
        .run(sched);
    
    sched->run_ready_tasks();
    sched->advance_time(1);
    sched->run_ready_tasks();
    sched->advance_time(1);
    sched->run_ready_tasks();
    sched->advance_time(1);
    sched->run_ready_tasks();
    sched->advance_time(1);
    sched->run_ready_tasks();

    auto result = fiber->await();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 4);
}

