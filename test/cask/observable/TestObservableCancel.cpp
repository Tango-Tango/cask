//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/None.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::BufferRef;
using cask::Observable;
using cask::Task;
using cask::scheduler::BenchScheduler;

TEST(ObservableCancel, CancelsWhenRun) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::cancel()->last().run(sched);

    sched->run_ready_tasks();

    EXPECT_TRUE(fiber->isCanceled());
}

TEST(ObservableCancel, CancelsWhenCanceled) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::cancel()->last().run(sched);

    fiber->cancel();
    sched->run_ready_tasks();

    EXPECT_TRUE(fiber->isCanceled());
}

TEST(ObservableCancel, FlatMapEmptyCancelDoesNothing) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::empty()
        ->template flatMap<int>([](auto) {
            return Observable<int>::cancel();
        })
        ->last()
        .run(sched);

    sched->run_ready_tasks();
    
    auto result = fiber->await();
    EXPECT_FALSE(result.has_value());
}

TEST(ObservableCancel, FlatMapValueCancels) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int>::pure(123)
        ->template flatMap<int>([](auto) {
            return Observable<int>::cancel();
        })
        ->last()
        .run(sched);

    sched->run_ready_tasks();
    
    EXPECT_TRUE(fiber->isCanceled());
}

TEST(ObservableCancel, FlatMapErrorSendsError) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Observable<int, std::string>::raiseError("broke")
        ->template flatMap<int>([](auto) {
            return Observable<int, std::string>::cancel();
        })
        ->last()
        .failed()
        .run(sched);

    sched->run_ready_tasks();


    auto error = fiber->await();
    EXPECT_EQ(error, "broke");
}
