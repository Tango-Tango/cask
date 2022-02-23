//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/MVar.hpp"
#include "cask/scheduler/BenchScheduler.hpp"
#include "gtest/gtest.h"

using cask::FiberRef;
using cask::MVar;
using cask::None;
using cask::Scheduler;
using cask::Task;

TEST(MVar, Empty) {
    auto sched = std::make_shared<cask::scheduler::BenchScheduler>();
    auto mvar = MVar<int, std::string>::empty(sched);

    auto takeOrTimeout =
        mvar->take().raceWith(Task<int, std::string>::raiseError("timeout").delay(1)).failed().run(sched);

    sched->run_ready_tasks();
    sched->advance_time(1);
    sched->run_ready_tasks();
    EXPECT_EQ(takeOrTimeout->await(), "timeout");
}

TEST(MVar, Create) {
    auto sched = std::make_shared<cask::scheduler::BenchScheduler>();
    auto mvar = MVar<int, std::string>::create(sched, 123);
    auto take = mvar->take().run(sched);
    sched->run_ready_tasks();
    EXPECT_EQ(take->await(), 123);
}

TEST(MVar, PutsAndTakes) {
    auto sched = std::make_shared<cask::scheduler::BenchScheduler>();
    auto mvar = MVar<int>::empty(sched);

    auto put = mvar->put(123).run(sched);
    auto take = mvar->take().run(sched);

    sched->run_ready_tasks();
    EXPECT_EQ(take->await(), 123);
    put->await();
}

TEST(MVar, ResolvesPendingTakesInOrder) {
    auto sched = std::make_shared<cask::scheduler::BenchScheduler>();
    auto mvar = MVar<int>::empty(sched);

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

TEST(MVar, ResolvesPendingPutsInOrder) {
    auto sched = std::make_shared<cask::scheduler::BenchScheduler>();
    auto mvar = MVar<int>::empty(sched);

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

TEST(MVar, InterleavePutsAndTakes) {
    auto sched = std::make_shared<cask::scheduler::BenchScheduler>();
    auto mvar = MVar<int>::empty(sched);

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

TEST(MVar, InterleavesTakesAndPuts) {
    auto sched = std::make_shared<cask::scheduler::BenchScheduler>();
    auto mvar = MVar<int>::empty(sched);

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

TEST(MVar, CleanupCanceledPut) {
    auto sched = std::make_shared<cask::scheduler::BenchScheduler>();
    auto mvar = MVar<int, std::string>::empty(sched);

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

TEST(MVar, CleanupCanceledTake) {
    auto sched = std::make_shared<cask::scheduler::BenchScheduler>();
    auto mvar = MVar<int, std::string>::empty(sched);

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

TEST(MVar, Read) {
    auto sched = std::make_shared<cask::scheduler::BenchScheduler>();
    auto mvar = MVar<int, std::string>::create(sched, 123);
    auto firstRead = mvar->read().run(sched);
    auto secondRead = mvar->read().run(sched);

    sched->run_ready_tasks();

    EXPECT_EQ(firstRead->await(), 123);
    EXPECT_EQ(secondRead->await(), 123);
}

TEST(MVar, ReadManyTimes) {
    auto sched = std::make_shared<cask::scheduler::BenchScheduler>();
    const static unsigned int iterations = 1000;
    std::vector<FiberRef<int, std::string>> reads;

    auto mvar = MVar<int, std::string>::create(sched, 123);
    for (unsigned int i = 0; i < iterations; i++) {
        auto deferred = mvar->read().run(sched);
        reads.push_back(deferred);
    }

    sched->run_ready_tasks();

    for (auto& fiber : reads) {
        EXPECT_EQ(fiber->await(), 123);
    }
}

TEST(MVar, TryPutEmpty) {
    auto sched = std::make_shared<cask::scheduler::BenchScheduler>();
    auto mvar = MVar<int, std::string>::empty(sched);

    ASSERT_TRUE(mvar->tryPut(1));
    ASSERT_FALSE(mvar->tryPut(2));

    auto firstTake = mvar->take().run(sched);

    sched->run_ready_tasks();

    EXPECT_EQ(firstTake->await(), 1);
}

TEST(MVar, TryPutPendingTakes) {
    auto sched = std::make_shared<cask::scheduler::BenchScheduler>();
    auto mvar = MVar<int, std::string>::empty(sched);

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

TEST(MVar, Modify) {
    auto sched = std::make_shared<cask::scheduler::BenchScheduler>();
    auto mvar = MVar<int, std::string>::create(sched, 123);

    auto fiber = mvar->template modify<float>([](auto value) {
                         int updated_state = value * 2;
                         float result = value * 1.5f;
                         std::tuple<int, float> both = {updated_state, result};
                         return Task<std::tuple<int, float>, std::string>::pure(both);
                     })
                     .run(sched);

    sched->run_ready_tasks();
    EXPECT_EQ(fiber->await(), 184.5);

    auto afterFiber = mvar->take().run(sched);
    sched->run_ready_tasks();
    EXPECT_EQ(afterFiber->await(), 246);
}