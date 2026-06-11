//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Deferred.hpp"
#include "cask/fiber/FiberOp.hpp"

using cask::Deferred;
using cask::Erased;
using cask::Promise;
using cask::fiber::FiberOp;

TEST(FiberOp, ValueCopy) {
    Erased value = 123;
    auto op = FiberOp::value(value);

    ASSERT_EQ(op->opType, cask::fiber::VALUE);
    EXPECT_EQ(op->data.constantData->template get<int>(), 123);
}

TEST(FiberOp, ValueMove) {
    auto op = FiberOp::value(Erased(123));

    ASSERT_EQ(op->opType, cask::fiber::VALUE);
    EXPECT_EQ(op->data.constantData->template get<int>(), 123);
}

TEST(FiberOp, Error) {
    Erased value = 123;
    auto op = FiberOp::error(value);

    ASSERT_EQ(op->opType, cask::fiber::ERROR);
    EXPECT_EQ(op->data.constantData->template get<int>(), 123);
}

TEST(FiberOp, Async) {
    auto op = FiberOp::async([](auto sched) {
        auto promise = Promise<Erased,Erased>::create(sched);
        return Deferred<Erased,Erased>::forPromise(promise);
    });

    ASSERT_EQ(op->opType, cask::fiber::ASYNC);

    auto function = *(op->data.asyncData);
    ASSERT_TRUE(function);
}

TEST(FiberOp, Thunk) {
    auto op = FiberOp::thunk([] {
        return Erased(123);
    });

    ASSERT_EQ(op->opType, cask::fiber::THUNK);

    auto function = *(op->data.thunkData);
    EXPECT_EQ(function().template get<int>(), 123);
}

TEST(FiberOp, Delay) {
    auto op = FiberOp::delay(1234);

    ASSERT_EQ(op->opType, cask::fiber::DELAY);

    auto delayedBy = *(op->data.delayData);
    EXPECT_EQ(delayedBy, 1234);
}

TEST(FiberOp, RaceCopy) {
    std::vector<cask::fiber::FiberOpRef> ops = {FiberOp::value(123), FiberOp::value(456)};
    auto op = FiberOp::race(ops);

    ASSERT_EQ(op->opType, cask::fiber::RACE);

    auto racingOps = *(op->data.raceData);
    EXPECT_EQ(racingOps.size(), 2);
}

TEST(FiberOp, RaceMove) {
    auto op = FiberOp::race({
        FiberOp::value(123),
        FiberOp::value(456)
    });

    ASSERT_EQ(op->opType, cask::fiber::RACE);

    auto racingOps = *(op->data.raceData);
    EXPECT_EQ(racingOps.size(), 2);
}

TEST(FiberOp, Cancel) {
    auto op = FiberOp::cancel();

    ASSERT_EQ(op->opType, cask::fiber::CANCEL);
}

TEST(FiberOp, FlatMap) {
    auto op = FiberOp::value(123)->flatMap([](auto value) {
        return FiberOp::value(value.underlying().template get<int>() * 2);
    });

    ASSERT_EQ(op->opType, cask::fiber::FLATMAP);

    auto input = op->data.flatMapData->first;
    auto predicate = op->data.flatMapData->second;

    ASSERT_EQ(input->opType, cask::fiber::VALUE);
    ASSERT_TRUE(predicate);
}

// --- FiberOpRef intrusive reference counting lifetime tests ---

namespace {

cask::fiber::FiberOpRef opHoldingToken(const std::shared_ptr<int>& token) {
    return FiberOp::thunk([token] {
        return Erased(*token);
    });
}

} // namespace

TEST(FiberOpRef, DestroysPayloadWhenLastRefDropped) {
    auto token = std::make_shared<int>(42);

    {
        auto op = opHoldingToken(token);
        EXPECT_GT(token.use_count(), 1);
    }

    EXPECT_EQ(token.use_count(), 1);
}

TEST(FiberOpRef, CopyExtendsLifetime) {
    auto token = std::make_shared<int>(42);

    auto op = opHoldingToken(token);
    auto copy = op;

    EXPECT_EQ(op.get(), copy.get());

    op = nullptr;
    EXPECT_EQ(op.get(), nullptr);
    EXPECT_GT(token.use_count(), 1);

    copy = nullptr;
    EXPECT_EQ(token.use_count(), 1);
}

TEST(FiberOpRef, MoveTransfersOwnership) {
    auto token = std::make_shared<int>(42);

    auto op = opHoldingToken(token);
    const auto* raw = op.get();

    auto moved = std::move(op);
    EXPECT_EQ(op.get(), nullptr); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move): intentionally checking moved-from state
    EXPECT_EQ(moved.get(), raw);
    EXPECT_GT(token.use_count(), 1);

    moved = nullptr;
    EXPECT_EQ(token.use_count(), 1);
}

TEST(FiberOpRef, CopyAssignReleasesPreviousTarget) {
    auto first_token = std::make_shared<int>(1);
    auto second_token = std::make_shared<int>(2);

    auto first = opHoldingToken(first_token);
    auto second = opHoldingToken(second_token);

    first = second;

    EXPECT_EQ(first_token.use_count(), 1);
    EXPECT_GT(second_token.use_count(), 1);
    EXPECT_EQ(first.get(), second.get());

    first = nullptr;
    second = nullptr;
    EXPECT_EQ(second_token.use_count(), 1);
}

TEST(FiberOpRef, MoveAssignReleasesPreviousTarget) {
    auto first_token = std::make_shared<int>(1);
    auto second_token = std::make_shared<int>(2);

    auto first = opHoldingToken(first_token);
    auto second = opHoldingToken(second_token);

    first = std::move(second);

    EXPECT_EQ(first_token.use_count(), 1);
    EXPECT_GT(second_token.use_count(), 1);
    EXPECT_EQ(second.get(), nullptr); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move): intentionally checking moved-from state

    first = nullptr;
    EXPECT_EQ(second_token.use_count(), 1);
}

TEST(FiberOpRef, SelfCopyAssignIsSafe) {
    auto token = std::make_shared<int>(42);

    auto op = opHoldingToken(token);
    const auto* raw = op.get();

    auto& alias = op;
    op = alias;

    EXPECT_EQ(op.get(), raw);
    EXPECT_GT(token.use_count(), 1);

    op = nullptr;
    EXPECT_EQ(token.use_count(), 1);
}

TEST(FiberOpRef, FlatMapRetainsInputOp) {
    auto token = std::make_shared<int>(42);

    auto input = opHoldingToken(token);
    auto composed = input->flatMap([](cask::fiber::FiberValue&& value) {
        return FiberOp::value(std::move(value).getValue().value_or(Erased(0)));
    });

    // Dropping the original handle must not destroy the input op - the
    // FLATMAP node holds its own reference to it.
    input = nullptr;
    EXPECT_GT(token.use_count(), 1);
    EXPECT_EQ(composed->data.flatMapData->first->opType, cask::fiber::THUNK);

    composed = nullptr;
    EXPECT_EQ(token.use_count(), 1);
}

TEST(FiberOpRef, RaceRetainsRacers) {
    auto first_token = std::make_shared<int>(1);
    auto second_token = std::make_shared<int>(2);

    auto race = FiberOp::race({
        opHoldingToken(first_token),
        opHoldingToken(second_token)
    });

    EXPECT_GT(first_token.use_count(), 1);
    EXPECT_GT(second_token.use_count(), 1);

    race = nullptr;
    EXPECT_EQ(first_token.use_count(), 1);
    EXPECT_EQ(second_token.use_count(), 1);
}
