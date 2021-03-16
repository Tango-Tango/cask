//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/MVar.hpp"

using cask::Task;
using cask::MVar;
using cask::Scheduler;

TEST(MVar, Empty) {
    auto mvar = MVar<int, std::string>::empty();

    auto takeOrTimeout = mvar->take()
        .raceWith(Task<float,std::string>::raiseError("timeout").delay(1))
        .failed()
        .run(Scheduler::global());

    EXPECT_EQ(takeOrTimeout->await(), "timeout");
}

TEST(MVar, Create) {
    auto mvar = MVar<int, std::string>::create(123);
    auto take = mvar->take().run(Scheduler::global());
    EXPECT_EQ(take->await(), 123);
}

TEST(MVar, PutsAndTakes) {
    auto mvar = MVar<int>::empty();

    auto put = mvar->put(123).run(Scheduler::global());
    auto take = mvar->take().run(Scheduler::global());

    EXPECT_EQ(take->await(), 123);
    put->await();
}




TEST(MVar, ResolvesPendingTakesInOrder) {
    auto mvar = MVar<int>::empty();

    auto firstTake = mvar->take().run(Scheduler::global());
    auto secondTake = mvar->take().run(Scheduler::global());
    
    auto firstPut = mvar->put(1).run(Scheduler::global());
    auto secondPut = mvar->put(2).run(Scheduler::global());

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(secondTake->await(), 2);

    firstPut->await();
    secondPut->await();
}

TEST(MVar, ResolvesPendingPutsInOrder) {
    auto mvar = MVar<int>::empty();

    auto firstPut = mvar->put(1).run(Scheduler::global());
    auto secondPut = mvar->put(2).run(Scheduler::global());
    auto thirdPut = mvar->put(3).run(Scheduler::global());

    auto firstTake = mvar->take().run(Scheduler::global());
    auto secondTake = mvar->take().run(Scheduler::global());
    auto thirdTake = mvar->take().run(Scheduler::global());

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(secondTake->await(), 2);
    EXPECT_EQ(thirdTake->await(), 3);
}

TEST(MVar, InterleavePutsAndTakes) {
    auto mvar = MVar<int>::empty();

    auto firstPut = mvar->put(1).run(Scheduler::global());
    auto firstTake = mvar->take().run(Scheduler::global());

    auto secondPut = mvar->put(2).run(Scheduler::global());
    auto secondTake = mvar->take().run(Scheduler::global());

    auto thirdPut = mvar->put(3).run(Scheduler::global());
    auto thirdTake = mvar->take().run(Scheduler::global());

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(secondTake->await(), 2);
    EXPECT_EQ(thirdTake->await(), 3);

    firstPut->await();
    secondPut->await();
    thirdPut->await();
}

TEST(MVar, InterleavesTakesAndPuts) {
    auto mvar = MVar<int>::empty();

    auto firstTake = mvar->take().run(Scheduler::global());
    auto firstPut = mvar->put(1).run(Scheduler::global());
    
    auto secondTake = mvar->take().run(Scheduler::global());
    auto secondPut = mvar->put(2).run(Scheduler::global());
    
    auto thirdTake = mvar->take().run(Scheduler::global());
    auto thirdPut = mvar->put(3).run(Scheduler::global());

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(secondTake->await(), 2);
    EXPECT_EQ(thirdTake->await(), 3);

    firstPut->await();
    secondPut->await();
    thirdPut->await();
}

TEST(MVar, CleanupCanceledPut) {
    auto mvar = MVar<int,std::string>::empty();

    auto firstPut = mvar->put(1).run(Scheduler::global());
    auto secondPut = mvar->put(2).run(Scheduler::global());
    auto thirdPut = mvar->put(3).run(Scheduler::global());

    secondPut->cancel("cancelled");

    auto firstTake = mvar->take().run(Scheduler::global());
    auto secondTake = mvar->take().run(Scheduler::global());

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(secondTake->await(), 3);

    firstPut->await();
    thirdPut->await();
}

TEST(MVar, CleanupCanceledTake) {
    auto mvar = MVar<int,std::string>::empty();

    auto firstTake = mvar->take().run(Scheduler::global());
    auto secondTake = mvar->take().run(Scheduler::global());
    auto thirdTake = mvar->take().run(Scheduler::global());

    secondTake->cancel("cancelled");

    auto firstPut = mvar->put(1).run(Scheduler::global());
    auto secondPut = mvar->put(2).run(Scheduler::global());
    auto thirdPut = mvar->put(3).run(Scheduler::global());

    EXPECT_EQ(firstTake->await(), 1);
    EXPECT_EQ(thirdTake->await(), 2);

    firstPut->await();
    secondPut->await();
    thirdPut->await();
}

TEST(MVar, Read) {
    auto mvar = MVar<int, std::string>::create(123);
    auto firstRead = mvar->read().run(Scheduler::global());
    auto secondRead = mvar->read().run(Scheduler::global());
    EXPECT_EQ(firstRead->await(), 123);
    EXPECT_EQ(secondRead->await(), 123);
}