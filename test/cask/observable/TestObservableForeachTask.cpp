//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::None;
using cask::Observable;
using cask::ObservableRef;
using cask::Scheduler;
using cask::Task;
using cask::scheduler::BenchScheduler;

TEST(ObservableForeachTask, Empty) {
    int counter = 0;

    Observable<int,std::string>::empty()
        ->foreachTask([&counter](auto) {
            counter++;
            return Task<None,std::string>::none();
        })
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(counter, 0);
}

TEST(ObservableForeachTask, SingleValue) {
    int counter = 0;

    Observable<int,std::string>::pure(567)
        ->foreachTask([&counter](auto) {
            counter++;
            return Task<None,std::string>::none();
        })
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(counter, 1);
}

TEST(ObservableForeachTask, MultipleValues) {
    int counter = 0;

    Observable<int,std::string>::sequence(5, 6, 7 ,3, 1, 4)
        ->foreachTask([&counter](auto) {
            counter++;
            return Task<None,std::string>::none();
        })
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(counter, 6);
}

TEST(ObservableForeachTask, RaiseError) {
    int counter = 0;

    auto result = Observable<int,std::string>::sequence(5, 6, 7 ,3, 1, 4)
        ->foreachTask([&counter](auto) {
            counter++;
            return Task<None,std::string>::raiseError("broke");
        })
        .failed()
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(counter, 1);
    EXPECT_EQ(result, "broke");
}

TEST(ObservableForeachTask, RaiseErrorConcat) {
    int counter = 0;

    auto result = Observable<int,std::string>::pure(1)
        ->concat(Observable<int,std::string>::pure(2))
        ->foreachTask([&counter](auto) {
            counter++;
            return Task<None,std::string>::raiseError("broke");
        })
        .failed()
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(counter, 1);
    EXPECT_EQ(result, "broke");
}

TEST(ObservableForeachTask, UpstreamError) {
    int counter = 0;

    auto result = Observable<int,std::string>::raiseError("already broke")
        ->foreachTask([&counter](auto) {
            counter++;
            return Task<None,std::string>::raiseError("broke");
        })
        .failed()
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(counter, 0);
    EXPECT_EQ(result, "already broke");
}

TEST(ObservableForeachTask, Canceled) {
    int counter = 0;

    auto deferred = Observable<int,std::string>::deferTask([] {
            return Task<int, std::string>::never();
        })
        ->foreachTask([&counter](auto) {
            counter++;
            return Task<None,std::string>::none();
        })
        .run(Scheduler::global());

    deferred->cancel();

    try {
        deferred->await();
        FAIL() << "Expected method to throw";
    } catch(std::runtime_error&) {
        EXPECT_EQ(counter, 0);
    }
}

TEST(ObservableForeachTask, CompletesGuaranteedEffects) {
    int counter = 0;
    bool completed = false;
    std::vector<int> values = {1,2,3,4,5};
    Observable<int,std::string>::fromVector(values)
        ->guarantee(
            Task<None,None>::eval([&completed] {
                completed = true;
                return None();
            }).delay(100)
        )
        ->foreachTask([&counter](auto) {
            counter++;
            return Task<None,std::string>::none();
        })
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(counter, 5);
    EXPECT_TRUE(completed);
}

TEST(ObservableForeachTask, RunsCancelCallbacks) {
    auto sched = std::make_shared<BenchScheduler>();
    int counter = 0;
    int run_count = 0;
    auto task = Task<None,None>::eval([&run_count]() {
        run_count++;
        return None();
    });

    auto fiber = Observable<int,std::string>::deferTask([]{
            return Task<int,std::string>::never();
        })
        ->guarantee(task)
        ->foreachTask([&counter](auto) {
            counter++;
            return Task<None,std::string>::none();
        })
        .run(sched);

    sched->run_ready_tasks();
    fiber->cancel();
    sched->run_ready_tasks();
    
    try {
        fiber->await();
        FAIL() << "Expected method to throw";
    } catch(std::runtime_error&) {
        EXPECT_EQ(run_count, 1);
    }
}
