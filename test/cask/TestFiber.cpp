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
using cask::scheduler::BenchScheduler;

TEST(TestFiber, Constructs) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::value(123);
    auto fiber = Fiber<int,std::string>::create(op);

    EXPECT_EQ(fiber->getState(), cask::READY);
    EXPECT_FALSE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
}

TEST(TestFiber, CancelsWhenDestructed) {
    cask::FiberState last_state;

    {
        auto sched = std::make_shared<BenchScheduler>();
        auto op = FiberOp::value(123);
        auto fiber = Fiber<int,std::string>::create(op);

        fiber->onShutdown([&last_state](auto fiber) {
            last_state = fiber->getState();
        });
    }

    EXPECT_EQ(last_state, cask::CANCELED);
}

TEST(TestFiber, DoesntCancelWhenCompletedAndDestructed) {
    cask::FiberState last_state;

    {
        auto sched = std::make_shared<BenchScheduler>();
        auto op = FiberOp::value(123);
        auto fiber = Fiber<int,std::string>::create(op);

        fiber->resumeSync();

        fiber->onShutdown([&last_state](auto fiber) {
            last_state = fiber->getState();
        });
    }

    EXPECT_EQ(last_state, cask::COMPLETED);
}


TEST(TestFiber, EvaluatesPureValue) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::value(123);
    auto fiber = Fiber<int,std::string>::create(op);
    
    EXPECT_TRUE(fiber->resume(sched));
    EXPECT_FALSE(fiber->resume(sched));

    EXPECT_EQ(fiber->getState(), cask::COMPLETED);
    EXPECT_TRUE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_EQ(*(fiber->getValue()), 123);
}

TEST(TestFiber, EvaluatesPureValueSync) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::value(123);
    auto fiber = Fiber<int,std::string>::create(op);
    
    EXPECT_TRUE(fiber->resumeSync());
    EXPECT_FALSE(fiber->resumeSync());

    EXPECT_EQ(fiber->getState(), cask::COMPLETED);
    EXPECT_TRUE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_EQ(*(fiber->getValue()), 123);
}

TEST(TestFiber, CallsShutdownCallback) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::value(123);
    auto fiber = Fiber<int,std::string>::create(op);
    int value = 0;
    int count = 0;
    
    fiber->onShutdown([&value, &count](auto fiber) {
        count++;
        value = *(fiber->getValue());
    });

    EXPECT_TRUE(fiber->resume(sched));
    EXPECT_FALSE(fiber->resume(sched));

    EXPECT_EQ(value, 123);
    EXPECT_EQ(count, 1);
}

TEST(TestFiber, CallsShutdownCallbackImmediately) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::value(123);
    auto fiber = Fiber<int,std::string>::create(op);
    int value = 0;
    int count = 0;

    EXPECT_TRUE(fiber->resume(sched));
    EXPECT_FALSE(fiber->resume(sched));

    fiber->onShutdown([&value, &count](auto fiber) {
        count++;
        value = *(fiber->getValue());
    });

    EXPECT_EQ(value, 123);
    EXPECT_EQ(count, 1);
}

TEST(TestFiber, SuspendsAtAsyncBoundary) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::async([](auto) {
        return Deferred<Erased,Erased>::pure(123);
    });
    auto fiber = Fiber<int,std::string>::create(op);

    EXPECT_TRUE(fiber->resume(sched));
    EXPECT_FALSE(fiber->resume(sched));

    EXPECT_EQ(fiber->getState(), cask::WAITING);
    EXPECT_FALSE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
}

TEST(TestFiber, ResumesAfterAsyncBoundary) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::async([](auto) {
        return Deferred<Erased,Erased>::pure(123);
    });
    auto fiber = Fiber<int,std::string>::create(op);

    EXPECT_TRUE(fiber->resume(sched));
    EXPECT_FALSE(fiber->resume(sched));

    sched->run_ready_tasks();

    EXPECT_EQ(fiber->getState(), cask::COMPLETED);
    EXPECT_TRUE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_EQ(*(fiber->getValue()), 123);
}

TEST(TestFiber, DelaysAValue) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::delay(10)->flatMap([](auto, auto) {
        return FiberOp::value(123);
    });
    auto fiber = Fiber<int,std::string>::create(op);

    // Run until the delay is hit
    EXPECT_TRUE(fiber->resume(sched));
    EXPECT_FALSE(fiber->resume(sched));
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

