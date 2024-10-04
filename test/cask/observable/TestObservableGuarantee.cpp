//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "gtest/trompeloeil.hpp"
#include "cask/Observable.hpp"
#include "cask/scheduler/BenchScheduler.hpp"
#include "cask/scheduler/SingleThreadScheduler.hpp"
#include "cask/scheduler/WorkStealingScheduler.hpp"

using cask::Observable;
using cask::ObservableRef;
using cask::Observer;
using cask::Scheduler;
using cask::Task;
using cask::None;
using cask::Ack;
using cask::observable::GuaranteeObserver;
using cask::scheduler::BenchScheduler;
using cask::scheduler::SingleThreadScheduler;
using cask::scheduler::WorkStealingScheduler;

class ObservableGuaranteeTest : public ::testing::TestWithParam<std::shared_ptr<Scheduler>> {
protected:

    void SetUp() override {
        sched = GetParam();
    }

    std::shared_ptr<Scheduler> sched;
};

class MockGuaranteeDownstreamObserver : public trompeloeil::mock_interface<Observer<int,float>> {
public:
    IMPLEMENT_MOCK1(onNext);
    IMPLEMENT_MOCK1(onError);
    IMPLEMENT_MOCK0(onComplete);
    IMPLEMENT_MOCK0(onCancel);
};

TEST_P(ObservableGuaranteeTest, RunsOnCompletion) {
    int run_count = 0;
    auto task = Task<None,None>::eval([&run_count]() {
        run_count++;
        return None();
    });

    auto result = Observable<int,float>::pure(123)
        ->guarantee(task)
        ->last()
        .run(sched)
        ->await();

    EXPECT_EQ(*result, 123);
    EXPECT_EQ(run_count, 1);
}

TEST_P(ObservableGuaranteeTest, RunsOnUpstreamComplete) {
    int run_count = 0;
    auto task = Task<None,None>::eval([&run_count]() {
        run_count++;
        return None();
    });

    auto result = Observable<int,float>::eval([]{ return 123; })
        ->guarantee(task)
        ->last()
        .run(sched)
        ->await();

    EXPECT_EQ(*result, 123);
    EXPECT_EQ(run_count, 1);
}

TEST_P(ObservableGuaranteeTest, RunsOnDownstreamStop) {
    int run_count = 0;
    auto task = Task<None,None>::eval([&run_count]() {
        run_count++;
        return None();
    });

    Observable<int,float>::repeatTask(Task<int,float>::pure(123))
        ->guarantee(task)
        ->takeWhile([](auto value) { return value != 123; })
        ->completed()
        .run(sched)
        ->await();

    EXPECT_EQ(run_count, 1);
}

TEST_P(ObservableGuaranteeTest, RunsOnError) {
    int run_count = 0;
    auto task = Task<None,None>::eval([&run_count]() {
        run_count++;
        return None();
    });

    auto result = Observable<int,float>::raiseError(1.23)
        ->guarantee(task)
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, 1.23f);
    EXPECT_EQ(run_count, 1);
}

TEST(ObservableGuaranteeTest, RunsOnSubscriptionCancel) {
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

TEST_P(ObservableGuaranteeTest, ErrorOnce) {
    int run_count = 0;
    auto task = Task<None,None>::eval([&run_count]() {
        run_count++;
        return None();
    });

    auto mockDownstream = std::make_shared<MockGuaranteeDownstreamObserver>();
    REQUIRE_CALL(*mockDownstream, onError(1.23f))
        .RETURN(Task<None,None>::none());

    auto observer = std::make_shared<GuaranteeObserver<int,float>>(mockDownstream, task);
    observer->onError(1.23).run(sched)->await();
    observer->onError(1.23).run(sched)->await();

    EXPECT_EQ(run_count, 1);
}

TEST_P(ObservableGuaranteeTest, CompleteOnce) {
    int run_count = 0;
    auto task = Task<None,None>::eval([&run_count]() {
        run_count++;
        return None();
    });

    auto mockDownstream = std::make_shared<MockGuaranteeDownstreamObserver>();
    REQUIRE_CALL(*mockDownstream, onComplete())
        .RETURN(Task<None,None>::none());

    auto observer = std::make_shared<GuaranteeObserver<int,float>>(mockDownstream, task);
    observer->onComplete().run(sched)->await();
    observer->onComplete().run(sched)->await();

    EXPECT_EQ(run_count, 1);
}

TEST_P(ObservableGuaranteeTest, CancelOnce) {
    int run_count = 0;
    auto task = Task<None,None>::eval([&run_count]() {
        run_count++;
        return None();
    });

    auto mockDownstream = std::make_shared<MockGuaranteeDownstreamObserver>();

    REQUIRE_CALL(*mockDownstream, onCancel())
        .RETURN(Task<None,None>::none());

    auto observer = std::make_shared<GuaranteeObserver<int,float>>(mockDownstream, task);
    observer->onCancel().run(sched)->await();
    observer->onCancel().run(sched)->await();

    EXPECT_EQ(run_count, 1);
}

INSTANTIATE_TEST_SUITE_P(ObservableGuaranteeTest, ObservableGuaranteeTest,
    ::testing::Values(
        std::make_shared<SingleThreadScheduler>(),
        std::make_shared<WorkStealingScheduler>(1),
        std::make_shared<WorkStealingScheduler>(2),
        std::make_shared<WorkStealingScheduler>(4),
        std::make_shared<WorkStealingScheduler>(8)
    ),
    [](const ::testing::TestParamInfo<ObservableGuaranteeTest::ParamType>& info) {
        return info.param->toString();
    }
);

