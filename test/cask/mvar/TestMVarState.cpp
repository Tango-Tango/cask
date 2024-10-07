//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/mvar/MVarState.hpp"
#include "cask/scheduler/BenchScheduler.hpp"
#include "SchedulerTestBench.hpp"

using cask::None;
using cask::mvar::MVarState;
using cask::Scheduler;

INSTANTIATE_SCHEDULER_TEST_BENCH_SUITE(MVarStateTest);

TEST_P(MVarStateTest, Empty) {
    auto initialState = MVarState<int,std::string>(sched);

    EXPECT_FALSE(initialState.valueOpt.has_value());
    EXPECT_TRUE(initialState.pendingPuts->is_empty());
    EXPECT_TRUE(initialState.pendingTakes->is_empty());
}

TEST_P(MVarStateTest, Initialized) {
    auto initialState = MVarState<int,std::string>(sched, 1);

    EXPECT_TRUE(initialState.valueOpt.has_value());
    EXPECT_TRUE(initialState.pendingPuts->is_empty());
    EXPECT_TRUE(initialState.pendingTakes->is_empty());
}

TEST_P(MVarStateTest, Put) {
    auto initialState = MVarState<int,std::string>(sched);
    auto result = initialState.put(1);
    const auto& state = std::get<0>(result);

    EXPECT_TRUE(state.valueOpt.has_value());
    EXPECT_TRUE(state.pendingPuts->is_empty());
    EXPECT_TRUE(state.pendingTakes->is_empty());
}

TEST_P(MVarStateTest, PutTake) {
    auto initialState = MVarState<int,std::string>(sched);
    auto [putState, putTask] = initialState.put(1);
    auto [takeState, takeTask] = putState.take();

    EXPECT_FALSE(takeState.valueOpt.has_value());
    EXPECT_TRUE(takeState.pendingPuts->is_empty());
    EXPECT_TRUE(takeState.pendingTakes->is_empty());

    auto putDeferred = putTask.run(sched);
    auto takeDeferred = takeTask.run(sched);

    EXPECT_EQ(putDeferred->await(), None());
    EXPECT_EQ(takeDeferred->await(), 1);
}

TEST_P(MVarStateTest, TakePut) {
    auto initialState = MVarState<int,std::string>(sched);
    auto [takeState, takeTask] = initialState.take();
    auto [putState, putTask] = takeState.put(1);
    
    EXPECT_FALSE(putState.valueOpt.has_value());
    EXPECT_TRUE(putState.pendingPuts->is_empty());
    EXPECT_TRUE(putState.pendingTakes->is_empty());

    auto putDeferred = putTask.run(sched);
    auto takeDeferred = takeTask.run(sched);

    EXPECT_EQ(putDeferred->await(), None());
    EXPECT_EQ(takeDeferred->await(), 1);
}

TEST_P(MVarStateTest, PutTakeTake) {
    auto initialState = MVarState<int,std::string>(sched);
    auto [putState, putTask] = initialState.put(1);
    auto [takeState, takeTask] = putState.take();
    auto result = takeState.take();
    const auto& secondTakeState = std::get<0>(result);

    EXPECT_FALSE(secondTakeState.valueOpt.has_value());
    EXPECT_TRUE(secondTakeState.pendingPuts->is_empty());
    EXPECT_FALSE(secondTakeState.pendingTakes->is_empty());

    auto putDeferred = putTask.run(sched);
    auto takeDeferred = takeTask.run(sched);

    EXPECT_EQ(putDeferred->await(), None());
    EXPECT_EQ(takeDeferred->await(), 1);
}

TEST_P(MVarStateTest, PutTakeTakePut) {
    auto initialState = MVarState<int,std::string>(sched);
    auto [putState, putTask] = initialState.put(1);
    auto [takeState, takeTask] = putState.take();
    auto [secondTakeState, secondTakeTask] = takeState.take();
    auto [secondPutState, secondPutTask] = secondTakeState.put(2);

    EXPECT_FALSE(secondPutState.valueOpt.has_value());
    EXPECT_TRUE(secondPutState.pendingPuts->is_empty());
    EXPECT_TRUE(secondPutState.pendingTakes->is_empty());

    auto putDeferred = putTask.run(sched);
    auto secondPutDeferred = secondPutTask.run(sched);
    auto takeDeferred = takeTask.run(sched);
    auto secondTakeDeferred = secondTakeTask.run(sched);

    EXPECT_EQ(putDeferred->await(), None());
    EXPECT_EQ(secondPutDeferred->await(), None());
    EXPECT_EQ(takeDeferred->await(), 1);
    EXPECT_EQ(secondTakeDeferred->await(), 2);
}

TEST_P(MVarStateTest, PutPutTakeTake) {
    auto initialState = MVarState<int,std::string>(sched);
    auto [putState, putTask] = initialState.put(1);
    auto [secondPutState, secondPutTask] = putState.put(2);
    auto [takeState, takeTask] = secondPutState.take();
    auto [secondTakeState, secondTakeTask] = takeState.take();
    
    EXPECT_FALSE(secondTakeState.valueOpt.has_value());
    EXPECT_TRUE(secondTakeState.pendingPuts->is_empty());
    EXPECT_TRUE(secondTakeState.pendingTakes->is_empty());

    auto putDeferred = putTask.run(sched);
    auto secondPutDeferred = secondPutTask.run(sched);
    auto takeDeferred = takeTask.run(sched);
    auto secondTakeDeferred = secondTakeTask.run(sched);

    EXPECT_EQ(putDeferred->await(), None());
    EXPECT_EQ(secondPutDeferred->await(), None());
    EXPECT_EQ(takeDeferred->await(), 1);
    EXPECT_EQ(secondTakeDeferred->await(), 2);
}

TEST_P(MVarStateTest, TakeTakePutPut) {
    auto initialState = MVarState<int,std::string>(sched);
    auto [takeState, takeTask] = initialState.take();
    auto [secondTakeState, secondTakeTask] = takeState.take();
    auto [putState, putTask] = secondTakeState.put(1);
    auto [secondPutState, secondPutTask] = putState.put(2);
    
    EXPECT_FALSE(secondPutState.valueOpt.has_value());
    EXPECT_TRUE(secondPutState.pendingPuts->is_empty());
    EXPECT_TRUE(secondPutState.pendingTakes->is_empty());

    auto putDeferred = putTask.run(sched);
    auto secondPutDeferred = secondPutTask.run(sched);
    auto takeDeferred = takeTask.run(sched);
    auto secondTakeDeferred = secondTakeTask.run(sched);

    EXPECT_EQ(putDeferred->await(), None());
    EXPECT_EQ(secondPutDeferred->await(), None());
    EXPECT_EQ(takeDeferred->await(), 1);
    EXPECT_EQ(secondTakeDeferred->await(), 2);
}
