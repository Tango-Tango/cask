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
    auto mvar = Queue<int, std::string>::empty(sched, 1);

    auto takeOrTimeout = mvar->take()
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
    auto mvar = Queue<int>::empty(sched, 1);

    auto put = mvar->put(123).run(sched);
    auto take = mvar->take().run(sched);

    sched->run_ready_tasks();

    EXPECT_EQ(take->await(), 123);
    put->await();
}

TEST(Queue, ResolvesPendingTakesInOrder) {
    auto sched = std::make_shared<BenchScheduler>();
    auto mvar = Queue<int>::empty(sched, 1);

    auto firstTake = mvar->take().run(sched);
    auto secondTake = mvar->take().run(sched);
    
    auto firstPut = mvar->put(1).run(sched);
    auto secondPut = mvar->put(2).run(sched);

    sched->run_ready_tasks();

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(secondTake->await(), 2);

    firstPut->await();
    secondPut->await();
}

TEST(Queue, ResolvesPendingPutsInOrder) {
    auto sched = std::make_shared<BenchScheduler>();
    auto mvar = Queue<int>::empty(sched, 1);

    auto firstPut = mvar->put(1).run(sched);
    auto secondPut = mvar->put(2).run(sched);
    auto thirdPut = mvar->put(3).run(sched);

    auto firstTake = mvar->take().run(sched);
    auto secondTake = mvar->take().run(sched);
    auto thirdTake = mvar->take().run(sched);

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
    auto mvar = Queue<int>::empty(sched, 1);

    auto firstPut = mvar->put(1).run(sched);
    auto firstTake = mvar->take().run(sched);

    auto secondPut = mvar->put(2).run(sched);
    auto secondTake = mvar->take().run(sched);

    auto thirdPut = mvar->put(3).run(sched);
    auto thirdTake = mvar->take().run(sched);

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
    auto mvar = Queue<int>::empty(sched, 1);

    auto firstTake = mvar->take().run(sched);
    auto firstPut = mvar->put(1).run(sched);
    
    auto secondTake = mvar->take().run(sched);
    auto secondPut = mvar->put(2).run(sched);
    
    auto thirdTake = mvar->take().run(sched);
    auto thirdPut = mvar->put(3).run(sched);

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
    auto mvar = Queue<int,std::string>::empty(sched, 1);

    auto firstPut = mvar->put(1).run(sched);
    auto secondPut = mvar->put(2).run(sched);
    auto thirdPut = mvar->put(3).run(sched);

    secondPut->cancel();

    auto firstTake = mvar->take().run(sched);
    auto secondTake = mvar->take().run(sched);

    sched->run_ready_tasks();

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(secondTake->await(), 3);

    EXPECT_EQ(firstPut->await(), None());
    EXPECT_EQ(thirdPut->await(), None());
}

TEST(Queue, CleanupCanceledTake) {
    auto sched = std::make_shared<BenchScheduler>();
    auto mvar = Queue<int,std::string>::empty(sched, 1);

    auto firstTake = mvar->take().run(sched);
    auto secondTake = mvar->take().run(sched);
    auto thirdTake = mvar->take().run(sched);

    secondTake->cancel();

    auto firstPut = mvar->put(1).run(sched);
    auto secondPut = mvar->put(2).run(sched);
    auto thirdPut = mvar->put(3).run(sched);

    sched->run_ready_tasks();

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(thirdTake->await(), 2);

    EXPECT_EQ(firstPut->await(), None());
    EXPECT_EQ(secondPut->await(), None());
    EXPECT_EQ(thirdPut->await(), None());
}

TEST(Queue, TryPutEmpty) {
    auto sched = std::make_shared<BenchScheduler>();
    auto mvar = Queue<int,std::string>::empty(sched, 1);

    ASSERT_TRUE(mvar->tryPut(1));
    ASSERT_FALSE(mvar->tryPut(2));

    auto firstTake = mvar->take().run(sched);
    sched->run_ready_tasks();

    EXPECT_EQ(firstTake->await(), 1);
}

TEST(Queue, TryPutFillQueue) {
    auto sched = std::make_shared<BenchScheduler>();
    auto mvar = Queue<int,std::string>::empty(sched, 5);

    for(unsigned int i = 0; i < 5; i++) {
        ASSERT_TRUE(mvar->tryPut(i));
    }

    ASSERT_FALSE(mvar->tryPut(6));

    for(unsigned int i = 0; i < 5; i++) {
        auto take = mvar->take().run(sched);
        sched->run_ready_tasks();
        ASSERT_EQ(take->await(), i);
    }
}

TEST(Queue, TryPutPendingTakes) {
    auto sched = std::make_shared<BenchScheduler>();
    auto mvar = Queue<int,std::string>::empty(sched, 1);

    auto firstTake = mvar->take().run(sched);
    auto secondTake = mvar->take().run(sched);
    auto thirdTake = mvar->take().run(sched);

    sched->run_ready_tasks();

    ASSERT_TRUE(mvar->tryPut(1));
    ASSERT_TRUE(mvar->tryPut(2));
    ASSERT_TRUE(mvar->tryPut(3));
    ASSERT_TRUE(mvar->tryPut(4));
    ASSERT_FALSE(mvar->tryPut(5));

    sched->run_ready_tasks();

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(secondTake->await(), 2);
    EXPECT_EQ(thirdTake->await(), 3);
}
