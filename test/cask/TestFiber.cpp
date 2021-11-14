//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Fiber.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::Deferred;
using cask::Fiber;
using cask::Erased;
using cask::FiberOp;
using cask::None;
using cask::Promise;
using cask::scheduler::BenchScheduler;

TEST(TestFiber, Constructs) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::value(123);
    auto fiber = Fiber<int,std::string>::run(op, sched);

    EXPECT_EQ(fiber->getState(), cask::READY);
    EXPECT_FALSE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
}

TEST(TestFiber, DoesntCancelWhenCompletedAndDestructed) {
    cask::FiberState last_state;

    {
        auto sched = std::make_shared<BenchScheduler>();
        auto op = FiberOp::value(123);
        auto fiber = Fiber<int,std::string>::run(op, sched);

        sched->run_ready_tasks();

        fiber->onShutdown([&last_state](auto fiber) {
            last_state = fiber->getState();
        });
    }

    EXPECT_EQ(last_state, cask::COMPLETED);
}


TEST(TestFiber, EvaluatesPureValue) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::value(123);
    auto fiber = Fiber<int,std::string>::run(op, sched);

    sched->run_ready_tasks();

    EXPECT_EQ(fiber->getState(), cask::COMPLETED);
    EXPECT_TRUE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_EQ(*(fiber->getValue()), 123);
}

TEST(TestFiber, EvaluatesPureValueSync) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::value(123);
    auto fiber = Fiber<int,std::string>::run(op, sched);

    sched->run_ready_tasks();

    EXPECT_EQ(fiber->getState(), cask::COMPLETED);
    EXPECT_TRUE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_EQ(*(fiber->getValue()), 123);
}

TEST(TestFiber, CallsShutdownCallback) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::value(123);
    auto fiber = Fiber<int,std::string>::run(op, sched);
    int value = 0;
    int count = 0;
    
    fiber->onShutdown([&value, &count](auto fiber) {
        count++;
        value = *(fiber->getValue());
    });

    sched->run_ready_tasks();

    EXPECT_EQ(value, 123);
    EXPECT_EQ(count, 1);
}

TEST(TestFiber, CallsShutdownCallbackImmediately) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::value(123);
    auto fiber = Fiber<int,std::string>::run(op, sched);
    int value = 0;
    int count = 0;

    sched->run_ready_tasks();

    fiber->onShutdown([&value, &count](auto fiber) {
        count++;
        value = *(fiber->getValue());
    });

    EXPECT_EQ(value, 123);
    EXPECT_EQ(count, 1);
}

TEST(TestFiber, SuspendsAtAsyncBoundary) {
    auto sched = std::make_shared<BenchScheduler>();
    auto promise = Promise<Erased,Erased>::create(sched);
    auto op = FiberOp::async([promise](auto) {
        return Deferred<Erased,Erased>::forPromise(promise);
    });
    auto fiber = Fiber<int,std::string>::run(op, sched);

    sched->run_ready_tasks();

    EXPECT_EQ(fiber->getState(), cask::WAITING);
    EXPECT_FALSE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
}

TEST(TestFiber, ResumesAfterAsyncBoundary) {
    auto sched = std::make_shared<BenchScheduler>();
    auto promise = Promise<Erased,Erased>::create(sched);
    auto op = FiberOp::async([promise](auto) {
        return Deferred<Erased,Erased>::forPromise(promise);
    });
    auto fiber = Fiber<int,std::string>::run(op, sched);

    sched->run_ready_tasks();

    promise->success(123);
    sched->run_ready_tasks();

    EXPECT_EQ(fiber->getState(), cask::COMPLETED);
    EXPECT_TRUE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_EQ(*(fiber->getValue()), 123);
}

TEST(TestFiber, DelaysAValue) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::delay(10)->flatMap([](auto) {
        return FiberOp::value(123);
    });
    auto fiber = Fiber<int,std::string>::run(op, sched);

    // Run until the delay is hit
    sched->run_ready_tasks();
    EXPECT_EQ(fiber->getState(), cask::DELAYED);

    // Trigger the delay to resolve
    sched->advance_time(10);
    sched->run_ready_tasks();
    EXPECT_EQ(fiber->getState(), cask::COMPLETED);
    
    // We should now have a result value
    EXPECT_TRUE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_EQ(*(fiber->getValue()), 123);
}

TEST(TestFiber, RacesSeveralOperationsFirstCompletes) {
    auto sched = std::make_shared<BenchScheduler>();
    auto promise1 = Promise<Erased,Erased>::create(sched);
    auto promise2 = Promise<Erased,Erased>::create(sched);
    auto promise3 = Promise<Erased,Erased>::create(sched);

    auto op1 = FiberOp::async([promise1](auto) {
        return Deferred<Erased,Erased>::forPromise(promise1);
    });
    auto op2 = FiberOp::async([promise2](auto) {
        return Deferred<Erased,Erased>::forPromise(promise2);
    });
    auto op3 = FiberOp::async([promise3](auto) {
        return Deferred<Erased,Erased>::forPromise(promise3);
    });
    auto race = FiberOp::race({op1, op2, op3});
    auto fiber = Fiber<int,std::string>::run(race, sched);

    sched->run_ready_tasks();

    EXPECT_EQ(fiber->getState(), cask::RACING);
    EXPECT_FALSE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());

    // Run the raced ops until they block on a value
    sched->run_ready_tasks();

    // Provide values and then run their callbacks on
    // the scheduler
    promise1->success(123);
    promise2->success(456);
    promise3->success(789);
    sched->run_ready_tasks();

    EXPECT_EQ(fiber->getState(), cask::COMPLETED);
    EXPECT_TRUE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_EQ(*(fiber->getValue()), 123);
}

TEST(TestFiber, RacesSeveralPureValues) {
    auto sched = std::make_shared<BenchScheduler>();
    auto promise1 = Promise<Erased,Erased>::create(sched);
    auto promise2 = Promise<Erased,Erased>::create(sched);
    auto promise3 = Promise<Erased,Erased>::create(sched);

    auto op1 = FiberOp::value(123);
    auto op2 = FiberOp::value(456);
    auto op3 = FiberOp::value(789);
    auto race = FiberOp::race({op1, op2, op3});
    auto fiber = Fiber<int,std::string>::run(race, sched);

    sched->run_ready_tasks();
    
    // Should immediately complete since these pure values
    // get immediately evaluated.
    EXPECT_EQ(fiber->getState(), cask::COMPLETED);
    EXPECT_TRUE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_EQ(*(fiber->getValue()), 123);
}

