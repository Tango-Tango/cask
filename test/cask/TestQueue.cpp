//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Queue.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::DeferredRef;
using cask::Task;
using cask::Queue;
using cask::Scheduler;
using cask::None;
using cask::scheduler::BenchScheduler;

TEST(Queue, Empty) {
    auto sched = std::make_shared<BenchScheduler>();
    auto queue = Queue<int, std::string>::empty(sched, 1);

    auto takeOrTimeout = queue->take()
        .raceWith(Task<int,std::string>::raiseError("timeout").delay(1))
        .failed()
        .run(sched);

    sched->run_ready_tasks();
    sched->advance_time(1);
    sched->run_ready_tasks();

    EXPECT_EQ(takeOrTimeout->await(), "timeout");
}

TEST(Queue, PutsAndTakes) {
    auto sched = std::make_shared<BenchScheduler>();
    auto queue = Queue<int>::empty(sched, 1);

    auto put = queue->put(123).run(sched);
    auto take = queue->take().run(sched);

    sched->run_ready_tasks();

    EXPECT_EQ(take->await(), 123);
    put->await();
}

TEST(Queue, ResolvesPendingTakesInOrder) {
    auto sched = std::make_shared<BenchScheduler>();
    auto queue = Queue<int>::empty(sched, 1);

    auto firstTake = queue->take().run(sched);
    auto secondTake = queue->take().run(sched);
    
    auto firstPut = queue->put(1).run(sched);
    auto secondPut = queue->put(2).run(sched);

    sched->run_ready_tasks();

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(secondTake->await(), 2);

    firstPut->await();
    secondPut->await();
}

TEST(Queue, ResolvesPendingPutsInOrder) {
    auto sched = std::make_shared<BenchScheduler>();
    auto queue = Queue<int>::empty(sched, 1);

    auto firstPut = queue->put(1).run(sched);
    auto secondPut = queue->put(2).run(sched);
    auto thirdPut = queue->put(3).run(sched);

    auto firstTake = queue->take().run(sched);
    auto secondTake = queue->take().run(sched);
    auto thirdTake = queue->take().run(sched);

    sched->run_ready_tasks();

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(secondTake->await(), 2);
    EXPECT_EQ(thirdTake->await(), 3);

    EXPECT_EQ(firstPut->await(), None());
    EXPECT_EQ(secondPut->await(), None());
    EXPECT_EQ(thirdPut->await(), None());
}

TEST(Queue, InterleavePutsAndTakes) {
    auto sched = std::make_shared<BenchScheduler>();
    auto queue = Queue<int>::empty(sched, 1);

    auto firstPut = queue->put(1).run(sched);
    auto firstTake = queue->take().run(sched);

    auto secondPut = queue->put(2).run(sched);
    auto secondTake = queue->take().run(sched);

    auto thirdPut = queue->put(3).run(sched);
    auto thirdTake = queue->take().run(sched);

    sched->run_ready_tasks();

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(secondTake->await(), 2);
    EXPECT_EQ(thirdTake->await(), 3);

    EXPECT_EQ(firstPut->await(), None());
    EXPECT_EQ(secondPut->await(), None());
    EXPECT_EQ(thirdPut->await(), None());
}

TEST(Queue, InterleavesTakesAndPuts) {
    auto sched = std::make_shared<BenchScheduler>();    
    auto queue = Queue<int>::empty(sched, 1);

    auto firstTake = queue->take().run(sched);
    auto firstPut = queue->put(1).run(sched);
    
    auto secondTake = queue->take().run(sched);
    auto secondPut = queue->put(2).run(sched);
    
    auto thirdTake = queue->take().run(sched);
    auto thirdPut = queue->put(3).run(sched);

    sched->run_ready_tasks();

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(secondTake->await(), 2);
    EXPECT_EQ(thirdTake->await(), 3);

    EXPECT_EQ(firstPut->await(), None());
    EXPECT_EQ(secondPut->await(), None());
    EXPECT_EQ(thirdPut->await(), None());
}

TEST(Queue, CleanupCanceledPut) {
    auto sched = std::make_shared<BenchScheduler>();
    auto queue = Queue<int,std::string>::empty(sched, 1);

    auto firstPut = queue->put(1).run(sched);
    auto secondPut = queue->put(2).run(sched);
    auto thirdPut = queue->put(3).run(sched);

    secondPut->cancel();

    auto firstTake = queue->take().run(sched);
    auto secondTake = queue->take().run(sched);

    sched->run_ready_tasks();

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(secondTake->await(), 3);

    EXPECT_EQ(firstPut->await(), None());
    EXPECT_EQ(thirdPut->await(), None());
}

TEST(Queue, CanceledPutShutdownCallback) {
    auto sched = std::make_shared<BenchScheduler>();
    auto queue = Queue<int,std::string>::empty(sched, 1);
    auto counter = 0;

    auto firstPut = queue->put(1).run(sched);
    auto secondPut = queue->put(2).run(sched);

    secondPut->onFiberShutdown([&counter](auto) {
        counter++;
    });

    secondPut->cancel();
    sched->run_ready_tasks();
    EXPECT_EQ(counter, 1);
}

TEST(Queue, CleanupCanceledTake) {
    auto sched = std::make_shared<BenchScheduler>();
    auto queue = Queue<int,std::string>::empty(sched, 1);

    auto firstTake = queue->take().run(sched);
    auto secondTake = queue->take().run(sched);
    auto thirdTake = queue->take().run(sched);

    secondTake->cancel();

    auto firstPut = queue->put(1).run(sched);
    auto secondPut = queue->put(2).run(sched);
    auto thirdPut = queue->put(3).run(sched);

    sched->run_ready_tasks();

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(thirdTake->await(), 2);

    EXPECT_EQ(firstPut->await(), None());
    EXPECT_EQ(secondPut->await(), None());
    EXPECT_EQ(thirdPut->await(), None());
}

TEST(Queue, CanceledTakeShutdownCallback) {
    auto sched = std::make_shared<BenchScheduler>();
    auto queue = Queue<int,std::string>::empty(sched, 1);
    auto counter = 0;

    auto firstTake = queue->take().asyncBoundary().run(sched);
    auto secondTake = queue->take().asyncBoundary().run(sched);

    secondTake->onFiberShutdown([&counter](auto) {
        counter++;
    });

    secondTake->cancel();
    sched->run_ready_tasks();
    EXPECT_EQ(counter, 1);
}

TEST(Queue, TryPutEmpty) {
    auto sched = std::make_shared<BenchScheduler>();
    auto queue = Queue<int,std::string>::empty(sched, 1);

    ASSERT_TRUE(queue->tryPut(1));
    ASSERT_FALSE(queue->tryPut(2));

    auto firstTake = queue->take().run(sched);
    sched->run_ready_tasks();

    EXPECT_EQ(firstTake->await(), 1);
}

TEST(Queue, TryPutFillQueue) {
    auto sched = std::make_shared<BenchScheduler>();
    auto queue = Queue<int,std::string>::empty(sched, 5);

    for(unsigned int i = 0; i < 5; i++) {
        ASSERT_TRUE(queue->tryPut(i));
    }

    ASSERT_FALSE(queue->tryPut(6));

    for(unsigned int i = 0; i < 5; i++) {
        auto take = queue->take().run(sched);
        sched->run_ready_tasks();
        ASSERT_EQ(take->await(), i);
    }
}

TEST(Queue, TryPutPendingTakes) {
    auto sched = std::make_shared<BenchScheduler>();
    auto queue = Queue<int,std::string>::empty(sched, 1);

    auto firstTake = queue->take().run(sched);
    auto secondTake = queue->take().run(sched);
    auto thirdTake = queue->take().run(sched);

    sched->run_ready_tasks();

    ASSERT_TRUE(queue->tryPut(1));
    ASSERT_TRUE(queue->tryPut(2));
    ASSERT_TRUE(queue->tryPut(3));
    ASSERT_TRUE(queue->tryPut(4));
    ASSERT_FALSE(queue->tryPut(5));

    sched->run_ready_tasks();

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(secondTake->await(), 2);
    EXPECT_EQ(thirdTake->await(), 3);
}

TEST(Queue, TryTakeEmpty) {
    auto sched = std::make_shared<BenchScheduler>();
    auto queue = Queue<int,std::string>::empty(sched, 1);

    ASSERT_FALSE(queue->tryTake().has_value());
}

TEST(Queue, TryTakeHasValue) {
    auto sched = std::make_shared<BenchScheduler>();
    auto queue = Queue<int,std::string>::empty(sched, 1);

    ASSERT_TRUE(queue->tryPut(1));

    auto value_opt = queue->tryTake();
    ASSERT_TRUE(value_opt.has_value());
    EXPECT_EQ(*value_opt, 1);
}

TEST(Queue, TryTakePendingPuts) {
    auto sched = std::make_shared<BenchScheduler>();
    auto queue = Queue<int,std::string>::empty(sched, 1);

    ASSERT_TRUE(queue->tryPut(1));
    auto pendingPut = queue->put(2).run(sched);

    sched->run_ready_tasks();

    {
        auto value_opt = queue->tryTake();
        ASSERT_TRUE(value_opt.has_value());
        EXPECT_EQ(*value_opt, 1);
    }

    {
        auto value_opt = queue->tryTake();
        ASSERT_TRUE(value_opt.has_value());
        EXPECT_EQ(*value_opt, 2);
    }

    sched->run_ready_tasks();
    pendingPut->await();
}

TEST(Queue, ResetPendingTake) {
    auto sched = std::make_shared<BenchScheduler>();
    auto queue = Queue<int, std::string>::empty(sched, 1);
    auto pending_take_fiber = queue->take().run(sched);

    sched->run_ready_tasks();
    queue->reset();
    sched->run_ready_tasks();

    EXPECT_TRUE(pending_take_fiber->isCanceled());
}

TEST(Queue, ResetPendingPut) {
    auto sched = std::make_shared<BenchScheduler>();
    auto queue = Queue<int, std::string>::empty(sched, 1);

    EXPECT_TRUE(queue->tryPut(0));

    auto pending_put_fiber = queue->put(1).run(sched);

    sched->run_ready_tasks();
    queue->reset();
    sched->run_ready_tasks();

    EXPECT_TRUE(pending_put_fiber->isCanceled());
}

TEST(Queue, ResetClearsValues) {
    auto sched = std::make_shared<BenchScheduler>();
    auto queue = Queue<int, std::string>::empty(sched, 1);

    EXPECT_TRUE(queue->tryPut(0));

    sched->run_ready_tasks();
    queue->reset();
    sched->run_ready_tasks();

    EXPECT_FALSE(queue->tryTake().has_value());
}
