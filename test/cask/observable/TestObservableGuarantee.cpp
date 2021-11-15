//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "gtest/trompeloeil.hpp"
#include "cask/Observable.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::Observable;
using cask::ObservableRef;
using cask::Observer;
using cask::Scheduler;
using cask::Task;
using cask::None;
using cask::Ack;
using cask::observable::GuaranteeObserver;
using cask::scheduler::BenchScheduler;

class MockGuaranteeDownstreamObserver : public trompeloeil::mock_interface<Observer<int,float>> {
public:
    IMPLEMENT_MOCK1(onNext);
    IMPLEMENT_MOCK1(onError);
    IMPLEMENT_MOCK0(onComplete);
    IMPLEMENT_MOCK0(onCancel);
};

TEST(ObservableGuarantee, RunsOnCompletion) {
    int run_count = 0;
    auto task = Task<None,None>::eval([&run_count]() {
        run_count++;
        return None();
    });

    auto result = Observable<int,float>::pure(123)
        ->guarantee(task)
        ->last()
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(*result, 123);
    EXPECT_EQ(run_count, 1);
}

TEST(ObservableGuarantee, RunsOnUpstreamComplete) {
    int run_count = 0;
    auto task = Task<None,None>::eval([&run_count]() {
        run_count++;
        return None();
    });

    auto result = Observable<int,float>::eval([]{ return 123; })
        ->guarantee(task)
        ->last()
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(*result, 123);
    EXPECT_EQ(run_count, 1);
}

TEST(ObservableGuarantee, RunsOnDownstreamStop) {
    int run_count = 0;
    auto task = Task<None,None>::eval([&run_count]() {
        run_count++;
        return None();
    });

    Observable<int,float>::repeatTask(Task<int,float>::pure(123))
        ->guarantee(task)
        ->takeWhile([](auto value) { return value != 123; })
        ->completed()
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(run_count, 1);
}

TEST(ObservableGuarantee, RunsOnError) {
    int run_count = 0;
    auto task = Task<None,None>::eval([&run_count]() {
        run_count++;
        return None();
    });

    auto result = Observable<int,float>::raiseError(1.23)
        ->guarantee(task)
        ->last()
        .failed()
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(result, 1.23f);
    EXPECT_EQ(run_count, 1);
}

TEST(ObservableGuarantee, RunsOnSubscriptionCancel) {
    auto sched = std::make_shared<BenchScheduler>();
    int run_count = 0;
    auto task = Task<None,None>::eval([&run_count]() {
        run_count++;
        return None();
    });

    auto deferred = Observable<int,float>::deferTask([]{
            return Task<int,float>::never();
        })
        ->guarantee(task)
        ->last()
        .run(sched);

    sched->run_ready_tasks();
    deferred->cancel();
    sched->run_ready_tasks();
    
    try {
        deferred->await();
        FAIL() << "Expected method to throw";
    } catch(std::runtime_error&) {
        EXPECT_EQ(run_count, 1);
    }
}

TEST(ObservableGuarantee, ErrorOnce) {
    int run_count = 0;
    auto task = Task<None,None>::eval([&run_count]() {
        run_count++;
        return None();
    });

    auto mockDownstream = std::make_shared<MockGuaranteeDownstreamObserver>();
    REQUIRE_CALL(*mockDownstream, onError(1.23f))
        .RETURN(Task<None,None>::none());

    auto observer = std::make_shared<GuaranteeObserver<int,float>>(mockDownstream, task);
    observer->onError(1.23).run(Scheduler::global())->await();
    observer->onError(1.23).run(Scheduler::global())->await();

    EXPECT_EQ(run_count, 1);
}

TEST(ObservableGuarantee, CompleteOnce) {
    int run_count = 0;
    auto task = Task<None,None>::eval([&run_count]() {
        run_count++;
        return None();
    });

    auto mockDownstream = std::make_shared<MockGuaranteeDownstreamObserver>();
    REQUIRE_CALL(*mockDownstream, onComplete())
        .RETURN(Task<None,None>::none());

    auto observer = std::make_shared<GuaranteeObserver<int,float>>(mockDownstream, task);
    observer->onComplete().run(Scheduler::global())->await();
    observer->onComplete().run(Scheduler::global())->await();

    EXPECT_EQ(run_count, 1);
}

TEST(ObservableGuarantee, CancelOnce) {
    int run_count = 0;
    auto task = Task<None,None>::eval([&run_count]() {
        run_count++;
        return None();
    });

    auto mockDownstream = std::make_shared<MockGuaranteeDownstreamObserver>();

    REQUIRE_CALL(*mockDownstream, onCancel())
        .RETURN(Task<None,None>::none());

    auto observer = std::make_shared<GuaranteeObserver<int,float>>(mockDownstream, task);
    observer->onCancel().run(Scheduler::global())->await();
    observer->onCancel().run(Scheduler::global())->await();

    EXPECT_EQ(run_count, 1);
}

