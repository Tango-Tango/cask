//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Fiber.hpp"
#include "cask/scheduler/BenchScheduler.hpp"
#include "cask/scheduler/SingleThreadScheduler.hpp"

using cask::Deferred;
using cask::Fiber;
using cask::FiberRef;
using cask::Erased;
using cask::None;
using cask::Promise;
using cask::fiber::FiberOp;
using cask::scheduler::BenchScheduler;
using cask::scheduler::SingleThreadScheduler;

TEST(TestFiber, Constructs) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::value(123);
    auto fiber = Fiber<int,std::string>::run(op, sched);

    EXPECT_FALSE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_FALSE(fiber->isCanceled());
}

TEST(TestFiber, DoesntCancelWhenCompletedAndDestructed) {
    bool finished = false;

    {
        auto sched = std::make_shared<BenchScheduler>();
        auto op = FiberOp::value(123);
        auto fiber = Fiber<int,std::string>::run(op, sched);

        sched->run_ready_tasks();

        fiber->onFiberShutdown([&finished](auto) {
            finished = true;
        });
    }

    EXPECT_TRUE(finished);
}


TEST(TestFiber, EvaluatesPureValue) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::value(123);
    auto fiber = Fiber<int,std::string>::run(op, sched);

    sched->run_ready_tasks();

    EXPECT_TRUE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_EQ(*(fiber->getValue()), 123);
}

TEST(TestFiber, EvaluatesPureValueSync) {
    auto op = FiberOp::value(123);
    auto result = Fiber<int,std::string>::runSync(op);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_left());
    ASSERT_EQ(result->get_left(), 123);
}

TEST(TestFiber, EvaluatesPureError) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::error(std::string("broke"));
    auto fiber = Fiber<int,std::string>::run(op, sched);

    sched->run_ready_tasks();

    EXPECT_FALSE(fiber->getValue().has_value());
    EXPECT_TRUE(fiber->getError().has_value());
    EXPECT_EQ(*(fiber->getError()), "broke");
}

TEST(TestFiber, EvaluatesPureErrorSync) {
    auto op = FiberOp::error(std::string("broke"));
    auto result = Fiber<int,std::string>::runSync(op);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_right());
    ASSERT_EQ(result->get_right(), "broke");
}

TEST(TestFiber, EvaluatesThunk) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::thunk([] {
        return 123;
    });
    auto fiber = Fiber<int,std::string>::run(op, sched);

    sched->run_ready_tasks();

    EXPECT_TRUE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_EQ(*(fiber->getValue()), 123);
}

TEST(TestFiber, EvaluatesThunkSync) {
    auto op = FiberOp::thunk([] {
        return 123;
    });
    auto result = Fiber<int,std::string>::runSync(op);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_left());
    ASSERT_EQ(result->get_left(), 123);
}

TEST(TestFiber, CallsShutdownCallback) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::value(123);
    auto fiber = Fiber<int,std::string>::run(op, sched);
    int value = 0;
    int count = 0;
    
    fiber->onFiberShutdown([&value, &count](auto fiber) {
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

    fiber->onFiberShutdown([&value, &count](auto fiber) {
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

    EXPECT_FALSE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_FALSE(fiber->isCanceled());
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

    EXPECT_TRUE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_EQ(*(fiber->getValue()), 123);
}

TEST(TestFiber, AsyncBoundarySync) {
    auto sched = std::make_shared<BenchScheduler>();
    auto promise = Promise<Erased,Erased>::create(sched);
    auto op = FiberOp::async([promise](auto) {
        return Deferred<Erased,Erased>::forPromise(promise);
    });
    auto result = Fiber<int,std::string>::runSync(op);
    EXPECT_FALSE(result.has_value());
}

TEST(TestFiber, DelaysAValue) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::delay(10)->flatMap([](auto) {
        return FiberOp::value(123);
    });
    auto fiber = Fiber<int,std::string>::run(op, sched);

    sched->run_ready_tasks();
    EXPECT_FALSE(fiber->getValue().has_value());

    sched->advance_time(10);
    sched->run_ready_tasks();
    
    EXPECT_TRUE(fiber->getValue().has_value());
    EXPECT_EQ(*(fiber->getValue()), 123);
}

TEST(TestFiber, DelaysAValueSync) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::delay(10)->flatMap([](auto) {
        return FiberOp::value(123);
    });
    auto result = Fiber<int,std::string>::runSync(op);
    EXPECT_FALSE(result.has_value());
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

    EXPECT_FALSE(fiber->getValue().has_value());

    sched->run_ready_tasks();
    promise1->success(123);
    promise2->success(456);
    promise3->success(789);
    sched->run_ready_tasks();

    EXPECT_TRUE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_EQ(*(fiber->getValue()), 123);
}

TEST(TestFiber, RacesSeveralPureValues) {
    auto sched = std::make_shared<BenchScheduler>();

    auto op1 = FiberOp::value(123);
    auto op2 = FiberOp::value(456);
    auto op3 = FiberOp::value(789);
    auto race = FiberOp::race({op1, op2, op3});
    auto fiber = Fiber<int,std::string>::run(race, sched);

    sched->run_ready_tasks();
    
    EXPECT_TRUE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_EQ(*(fiber->getValue()), 123);
}

TEST(TestFiber, RacesSeveralPureValuesSync) {
    auto op1 = FiberOp::value(123);
    auto op2 = FiberOp::value(456);
    auto op3 = FiberOp::value(789);
    auto race = FiberOp::race({op1, op2, op3});
    auto result = Fiber<int,std::string>::runSync(race);
    
    EXPECT_FALSE(result.has_value());
}

TEST(TestFiber, RacesAndCancels) {
    auto sched = std::make_shared<BenchScheduler>();
    auto promise1 = Promise<Erased,Erased>::create(sched);
    auto promise2 = Promise<Erased,Erased>::create(sched);
    auto promise3 = Promise<Erased,Erased>::create(sched);

    bool promise1_canceled = false;
    bool promise2_canceled = false;
    bool promise3_canceled = false;

    auto op1 = FiberOp::async([promise1](auto) {
        return Deferred<Erased,Erased>::forPromise(promise1);
    })->flatMap([&promise1_canceled](auto fiber_value) {
        promise1_canceled = fiber_value.isCanceled();
        return FiberOp::value(fiber_value.isCanceled());
    });

    auto op2 = FiberOp::async([promise2](auto) {
        return Deferred<Erased,Erased>::forPromise(promise2);
    })->flatMap([&promise2_canceled](auto fiber_value) {
        promise2_canceled = fiber_value.isCanceled();
        return FiberOp::value(fiber_value.isCanceled());
    });

    auto op3 = FiberOp::async([promise3](auto) {
        return Deferred<Erased,Erased>::forPromise(promise3);
    })->flatMap([&promise3_canceled](auto fiber_value) {
        promise3_canceled = fiber_value.isCanceled();
        return FiberOp::value(fiber_value.isCanceled());
    });

    auto race = FiberOp::race({op1, op2, op3});
    auto fiber = Fiber<bool,std::string>::run(race, sched);

    sched->run_ready_tasks();
    fiber->cancel();
    sched->run_ready_tasks();
    promise1->success(123);
    promise2->success(456);
    promise3->success(789);
    sched->run_ready_tasks();

    EXPECT_TRUE(fiber->isCanceled());
    EXPECT_TRUE(promise1_canceled);
    EXPECT_TRUE(promise2_canceled);
    EXPECT_TRUE(promise3_canceled);
}

TEST(TestFiber, MapBothValue) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::value(123);
    auto fiber = Fiber<int,std::string>::run(op, sched)
        ->template mapBoth<std::string,int>(
            [](auto) { return "success"; },
            [](auto) { return 1337; }
        );

    sched->run_ready_tasks();

    EXPECT_TRUE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_EQ(*(fiber->getValue()), "success");
}

TEST(TestFiber, MapBothError) {
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::error(std::string("broke"));
    auto fiber = Fiber<int,std::string>::run(op, sched)
        ->template mapBoth<std::string,int>(
            [](auto) { return "success"; },
            [](auto) { return 1337; }
        );

    sched->run_ready_tasks();

    EXPECT_TRUE(fiber->getError().has_value());
    EXPECT_EQ(*(fiber->getError()), 1337);
}

TEST(TestFiber, CurrentIdNone) {
    auto current_id = Fiber<int,std::string>::currentFiberId();
    EXPECT_FALSE(current_id.has_value());
}

TEST(TestFiber, CurrentId){
    auto sched = std::make_shared<BenchScheduler>();
    auto op = FiberOp::thunk([] {
        return Fiber<int,std::string>::currentFiberId();
    });
    auto fiber = Fiber<std::optional<uint64_t>,std::string>::run(op, sched);

    sched->run_ready_tasks();

    EXPECT_TRUE(fiber->getValue().has_value());
    EXPECT_FALSE(fiber->getError().has_value());
    EXPECT_EQ(*(fiber->getValue()), fiber->getId());

    auto current_id = Fiber<int,std::string>::currentFiberId();
    EXPECT_FALSE(current_id.has_value());
}
