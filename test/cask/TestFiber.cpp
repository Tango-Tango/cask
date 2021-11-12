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

