//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/mvar/MVarState.hpp"

using cask::None;
using cask::Scheduler;
using cask::mvar::MVarState;

TEST(MVarState, Empty) {
    auto initialState = MVarState<int,std::string>(Scheduler::global());

    EXPECT_FALSE(initialState.valueOpt.has_value());
    EXPECT_TRUE(initialState.pendingPuts->is_empty());
    EXPECT_TRUE(initialState.pendingTakes->is_empty());
}

TEST(MVarState, Initialized) {
    auto initialState = MVarState<int,std::string>(Scheduler::global(), 1);

    EXPECT_TRUE(initialState.valueOpt.has_value());
    EXPECT_TRUE(initialState.pendingPuts->is_empty());
    EXPECT_TRUE(initialState.pendingTakes->is_empty());
}

TEST(MVarState, Put) {
    auto initialState = MVarState<int,std::string>(Scheduler::global());
    auto result = initialState.put(1);
    const auto& state = std::get<0>(result);

    EXPECT_TRUE(state.valueOpt.has_value());
    EXPECT_TRUE(state.pendingPuts->is_empty());
    EXPECT_TRUE(state.pendingTakes->is_empty());
}

TEST(MVarState, PutTake) {
    auto initialState = MVarState<int,std::string>(Scheduler::global());
    auto [putState, putTask] = initialState.put(1);
    auto [takeState, takeTask] = putState.take();

    EXPECT_FALSE(takeState.valueOpt.has_value());
    EXPECT_TRUE(takeState.pendingPuts->is_empty());
    EXPECT_TRUE(takeState.pendingTakes->is_empty());

    auto putDeferred = putTask.run(Scheduler::global());
    auto takeDeferred = takeTask.run(Scheduler::global());

    EXPECT_EQ(putDeferred->await(), None());
    EXPECT_EQ(takeDeferred->await(), 1);
}

TEST(MVarState, TakePut) {
    auto initialState = MVarState<int,std::string>(Scheduler::global());
    auto [takeState, takeTask] = initialState.take();
    auto [putState, putTask] = takeState.put(1);
    
    EXPECT_FALSE(putState.valueOpt.has_value());
    EXPECT_TRUE(putState.pendingPuts->is_empty());
    EXPECT_TRUE(putState.pendingTakes->is_empty());

    auto putDeferred = putTask.run(Scheduler::global());
    auto takeDeferred = takeTask.run(Scheduler::global());

    EXPECT_EQ(putDeferred->await(), None());
    EXPECT_EQ(takeDeferred->await(), 1);
}

TEST(MVarState, PutTakeTake) {
    auto initialState = MVarState<int,std::string>(Scheduler::global());
    auto [putState, putTask] = initialState.put(1);
    auto [takeState, takeTask] = putState.take();
    auto result = takeState.take();
    const auto& secondTakeState = std::get<0>(result);

    EXPECT_FALSE(secondTakeState.valueOpt.has_value());
    EXPECT_TRUE(secondTakeState.pendingPuts->is_empty());
    EXPECT_FALSE(secondTakeState.pendingTakes->is_empty());

    auto putDeferred = putTask.run(Scheduler::global());
    auto takeDeferred = takeTask.run(Scheduler::global());

    EXPECT_EQ(putDeferred->await(), None());
    EXPECT_EQ(takeDeferred->await(), 1);
}

TEST(MVarState, PutTakeTakePut) {
    auto initialState = MVarState<int,std::string>(Scheduler::global());
    auto [putState, putTask] = initialState.put(1);
    auto [takeState, takeTask] = putState.take();
    auto [secondTakeState, secondTakeTask] = takeState.take();
    auto [secondPutState, secondPutTask] = secondTakeState.put(2);

    EXPECT_FALSE(secondPutState.valueOpt.has_value());
    EXPECT_TRUE(secondPutState.pendingPuts->is_empty());
    EXPECT_TRUE(secondPutState.pendingTakes->is_empty());

    auto putDeferred = putTask.run(Scheduler::global());
    auto secondPutDeferred = secondPutTask.run(Scheduler::global());
    auto takeDeferred = takeTask.run(Scheduler::global());
    auto secondTakeDeferred = secondTakeTask.run(Scheduler::global());

    EXPECT_EQ(putDeferred->await(), None());
    EXPECT_EQ(secondPutDeferred->await(), None());
    EXPECT_EQ(takeDeferred->await(), 1);
    EXPECT_EQ(secondTakeDeferred->await(), 2);
}

TEST(MVarState, PutPutTakeTake) {
    auto initialState = MVarState<int,std::string>(Scheduler::global());
    auto [putState, putTask] = initialState.put(1);
    auto [secondPutState, secondPutTask] = putState.put(2);
    auto [takeState, takeTask] = secondPutState.take();
    auto [secondTakeState, secondTakeTask] = takeState.take();
    
    EXPECT_FALSE(secondTakeState.valueOpt.has_value());
    EXPECT_TRUE(secondTakeState.pendingPuts->is_empty());
    EXPECT_TRUE(secondTakeState.pendingTakes->is_empty());

    auto putDeferred = putTask.run(Scheduler::global());
    auto secondPutDeferred = secondPutTask.run(Scheduler::global());
    auto takeDeferred = takeTask.run(Scheduler::global());
    auto secondTakeDeferred = secondTakeTask.run(Scheduler::global());

    EXPECT_EQ(putDeferred->await(), None());
    EXPECT_EQ(secondPutDeferred->await(), None());
    EXPECT_EQ(takeDeferred->await(), 1);
    EXPECT_EQ(secondTakeDeferred->await(), 2);
}

TEST(MVarState, TakeTakePutPut) {
    auto initialState = MVarState<int,std::string>(Scheduler::global());
    auto [takeState, takeTask] = initialState.take();
    auto [secondTakeState, secondTakeTask] = takeState.take();
    auto [putState, putTask] = secondTakeState.put(1);
    auto [secondPutState, secondPutTask] = putState.put(2);
    
    EXPECT_FALSE(secondPutState.valueOpt.has_value());
    EXPECT_TRUE(secondPutState.pendingPuts->is_empty());
    EXPECT_TRUE(secondPutState.pendingTakes->is_empty());

    auto putDeferred = putTask.run(Scheduler::global());
    auto secondPutDeferred = secondPutTask.run(Scheduler::global());
    auto takeDeferred = takeTask.run(Scheduler::global());
    auto secondTakeDeferred = secondTakeTask.run(Scheduler::global());

    EXPECT_EQ(putDeferred->await(), None());
    EXPECT_EQ(secondPutDeferred->await(), None());
    EXPECT_EQ(takeDeferred->await(), 1);
    EXPECT_EQ(secondTakeDeferred->await(), 2);
}
