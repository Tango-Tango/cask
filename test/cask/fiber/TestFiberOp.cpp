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
    std::vector<std::shared_ptr<const FiberOp>> ops = {FiberOp::value(123), FiberOp::value(456)};
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
