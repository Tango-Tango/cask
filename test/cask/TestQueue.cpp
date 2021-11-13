//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Queue.hpp"

using cask::DeferredRef;
using cask::Task;
using cask::Queue;
using cask::Scheduler;
using cask::None;

TEST(Queue, Empty) {
    auto mvar = Queue<int, std::string>::empty(Scheduler::global(), 1);

    auto takeOrTimeout = mvar->take()
        .raceWith(Task<int,std::string>::raiseError("timeout").delay(1))
        .failed()
        .run(Scheduler::global());

    EXPECT_EQ(takeOrTimeout->await(), "timeout");
}

TEST(Queue, PutsAndTakes) {
    auto mvar = Queue<int>::empty(Scheduler::global(), 1);

    auto put = mvar->put(123).run(Scheduler::global());
    auto take = mvar->take().run(Scheduler::global());

    EXPECT_EQ(take->await(), 123);
    put->await();
}

TEST(Queue, ResolvesPendingTakesInOrder) {
    auto mvar = Queue<int>::empty(Scheduler::global(), 1);

    auto firstTake = mvar->take().run(Scheduler::global());
    auto secondTake = mvar->take().run(Scheduler::global());
    
    auto firstPut = mvar->put(1).run(Scheduler::global());
    auto secondPut = mvar->put(2).run(Scheduler::global());

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(secondTake->await(), 2);

    firstPut->await();
    secondPut->await();
}

TEST(Queue, ResolvesPendingPutsInOrder) {
    auto mvar = Queue<int>::empty(Scheduler::global(), 1);

    auto firstPut = mvar->put(1).run(Scheduler::global());
    auto secondPut = mvar->put(2).run(Scheduler::global());
    auto thirdPut = mvar->put(3).run(Scheduler::global());

    auto firstTake = mvar->take().run(Scheduler::global());
    auto secondTake = mvar->take().run(Scheduler::global());
    auto thirdTake = mvar->take().run(Scheduler::global());

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(secondTake->await(), 2);
    EXPECT_EQ(thirdTake->await(), 3);

    EXPECT_EQ(firstPut->await(), None());
    EXPECT_EQ(secondPut->await(), None());
    EXPECT_EQ(thirdPut->await(), None());
}

TEST(Queue, InterleavePutsAndTakes) {
    auto mvar = Queue<int>::empty(Scheduler::global(), 1);

    auto firstPut = mvar->put(1).run(Scheduler::global());
    auto firstTake = mvar->take().run(Scheduler::global());

    auto secondPut = mvar->put(2).run(Scheduler::global());
    auto secondTake = mvar->take().run(Scheduler::global());

    auto thirdPut = mvar->put(3).run(Scheduler::global());
    auto thirdTake = mvar->take().run(Scheduler::global());

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(secondTake->await(), 2);
    EXPECT_EQ(thirdTake->await(), 3);

    EXPECT_EQ(firstPut->await(), None());
    EXPECT_EQ(secondPut->await(), None());
    EXPECT_EQ(thirdPut->await(), None());
}

TEST(Queue, InterleavesTakesAndPuts) {
    auto mvar = Queue<int>::empty(Scheduler::global(), 1);

    auto firstTake = mvar->take().run(Scheduler::global());
    auto firstPut = mvar->put(1).run(Scheduler::global());
    
    auto secondTake = mvar->take().run(Scheduler::global());
    auto secondPut = mvar->put(2).run(Scheduler::global());
    
    auto thirdTake = mvar->take().run(Scheduler::global());
    auto thirdPut = mvar->put(3).run(Scheduler::global());

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(secondTake->await(), 2);
    EXPECT_EQ(thirdTake->await(), 3);

    EXPECT_EQ(firstPut->await(), None());
    EXPECT_EQ(secondPut->await(), None());
    EXPECT_EQ(thirdPut->await(), None());
}

TEST(Queue, CleanupCanceledPut) {
    auto mvar = Queue<int,std::string>::empty(Scheduler::global(), 1);

    auto firstPut = mvar->put(1).run(Scheduler::global());
    auto secondPut = mvar->put(2).run(Scheduler::global());
    auto thirdPut = mvar->put(3).run(Scheduler::global());

    secondPut->cancel();

    auto firstTake = mvar->take().run(Scheduler::global());
    auto secondTake = mvar->take().run(Scheduler::global());

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(secondTake->await(), 3);

    EXPECT_EQ(firstPut->await(), None());
    EXPECT_EQ(thirdPut->await(), None());
}

TEST(Queue, CleanupCanceledTake) {
    auto mvar = Queue<int,std::string>::empty(Scheduler::global(), 1);

    auto firstTake = mvar->take().run(Scheduler::global());
    auto secondTake = mvar->take().run(Scheduler::global());
    auto thirdTake = mvar->take().run(Scheduler::global());

    secondTake->cancel();

    auto firstPut = mvar->put(1).run(Scheduler::global());
    auto secondPut = mvar->put(2).run(Scheduler::global());
    auto thirdPut = mvar->put(3).run(Scheduler::global());

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(thirdTake->await(), 2);

    EXPECT_EQ(firstPut->await(), None());
    EXPECT_EQ(secondPut->await(), None());
    EXPECT_EQ(thirdPut->await(), None());
}

TEST(Queue, TryPutEmpty) {
    auto mvar = Queue<int,std::string>::empty(Scheduler::global(), 1);

    ASSERT_TRUE(mvar->tryPut(1));
    ASSERT_FALSE(mvar->tryPut(2));

    auto firstTake = mvar->take().run(Scheduler::global())->await();
    EXPECT_EQ(firstTake, 1);
}

TEST(Queue, TryPutFillQueue) {
    auto mvar = Queue<int,std::string>::empty(Scheduler::global(), 5);

    for(unsigned int i = 0; i < 5; i++) {
        ASSERT_TRUE(mvar->tryPut(i));
    }

    ASSERT_FALSE(mvar->tryPut(6));

    for(unsigned int i = 0; i < 5; i++) {
        auto take = mvar->take().run(Scheduler::global())->await();
        ASSERT_EQ(take, i);
    }
}

TEST(Queue, TryPutPendingTakes) {
    auto mvar = Queue<int,std::string>::empty(Scheduler::global(), 1);

    auto firstTake = mvar->take().run(Scheduler::global());
    auto secondTake = mvar->take().run(Scheduler::global());
    auto thirdTake = mvar->take().run(Scheduler::global());

    ASSERT_TRUE(mvar->tryPut(1));
    ASSERT_TRUE(mvar->tryPut(2));
    ASSERT_TRUE(mvar->tryPut(3));
    ASSERT_TRUE(mvar->tryPut(4));
    ASSERT_FALSE(mvar->tryPut(5));

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(secondTake->await(), 2);
    EXPECT_EQ(thirdTake->await(), 3);
}
