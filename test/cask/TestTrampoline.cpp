//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/trampoline/TrampolineOp.hpp"
#include "cask/trampoline/TrampolineRunLoop.hpp"

using cask::Deferred;
using cask::DeferredRef;
using cask::Either;
using cask::Scheduler;
using cask::trampoline::TrampolineOp;
using cask::trampoline::TrampolineRunLoop;

TEST(Trampoline,Value) {
    auto op = TrampolineOp::value(123);
    auto result = TrampolineRunLoop::execute(op, Scheduler::global());
    auto syncResult = std::get<Either<std::any,std::any>>(result);
    auto value = std::any_cast<int>(syncResult.get_left());
    EXPECT_EQ(value, 123);
}

TEST(Trampoline,ValueAssignment) {
    auto op = TrampolineOp::value(123);
    TrampolineOp op2 = *op;
    EXPECT_EQ(op->opType, op2.opType);

    auto left = op->data.constantData->get_left();
    auto right = op2.data.constantData->get_left();
    EXPECT_EQ(std::any_cast<int>(left), std::any_cast<int>(right));
}

TEST(Trampoline,ValueMove) {
    auto op = TrampolineOp::value(123);
    TrampolineOp op2 = *op;
    TrampolineOp op3 = std::move(op2);
    EXPECT_EQ(op->opType, op3.opType);

    auto left = op->data.constantData->get_left();
    auto right = op3.data.constantData->get_left();
    EXPECT_EQ(std::any_cast<int>(left), std::any_cast<int>(right));
}

TEST(Trampoline,Error) {
    auto op = TrampolineOp::error(123);
    auto result = TrampolineRunLoop::execute(op, Scheduler::global());
    auto syncResult = std::get<Either<std::any,std::any>>(result);
    auto value = std::any_cast<int>(syncResult.get_right());
    EXPECT_EQ(value, 123);
}

TEST(Trampoline,ErrorAssignment) {
    auto op = TrampolineOp::error(123);
    TrampolineOp op2 = *op;
    EXPECT_EQ(op->opType, op2.opType);

    auto left = op->data.constantData->get_right();
    auto right = op2.data.constantData->get_right();
    EXPECT_EQ(std::any_cast<int>(left), std::any_cast<int>(right));
}

TEST(Trampoline,ErrorMove) {
    auto op = TrampolineOp::error(123);
    TrampolineOp op2 = *op;
    TrampolineOp op3 = std::move(op2);
    EXPECT_EQ(op->opType, op3.opType);

    auto left = op->data.constantData->get_right();
    auto right = op3.data.constantData->get_right();
    EXPECT_EQ(std::any_cast<int>(left), std::any_cast<int>(right));
}

TEST(Trampoline,Thunk) {
    auto op = TrampolineOp::thunk([]() { return 123; });
    auto result = TrampolineRunLoop::execute(op, Scheduler::global());
    auto syncResult = std::get<Either<std::any,std::any>>(result);
    auto value = std::any_cast<int>(syncResult.get_left());
    EXPECT_EQ(value, 123);
}

TEST(Trampoline,ThunkAssignment) {
    auto op = TrampolineOp::thunk([]() { return 123; });
    TrampolineOp op2 = *op;
    EXPECT_EQ(op->opType, op2.opType);

    auto left = op->data.thunkData->target<std::any(*)()>();
    auto right = op2.data.thunkData->target<std::any(*)()>();
    EXPECT_EQ(left, right);
}

TEST(Trampoline,ThunkMove) {
    auto op = TrampolineOp::thunk([]() { return 123; });
    TrampolineOp op2 = *op;
    TrampolineOp op3 = std::move(op2);
    EXPECT_EQ(op->opType, op3.opType);

    auto left = op->data.thunkData->target<std::any(*)()>();
    auto right = op3.data.thunkData->target<std::any(*)()>();
    EXPECT_EQ(left, right);
}

TEST(Trampoline,AsyncValue) {
    auto op = TrampolineOp::async([](auto sched) {
        return Deferred<std::any,std::any>::pure(123);
    });

    auto result = TrampolineRunLoop::execute(op, Scheduler::global());
    auto asyncResult = std::get<DeferredRef<std::any,std::any>>(result);
    auto awaitResult = asyncResult->await();
    auto value = std::any_cast<int>(awaitResult);
    EXPECT_EQ(value, 123);
}

TEST(Trampoline,AsyncError) {
    auto op = TrampolineOp::async([](auto sched) {
        return Deferred<std::any,std::any>::raiseError(123);
    });

    auto result = TrampolineRunLoop::execute(op, Scheduler::global());
    auto asyncResult = std::get<DeferredRef<std::any,std::any>>(result);

    try {
        asyncResult->await();
        FAIL() << "Exceted function to throw";
    } catch(std::any& error) {
        EXPECT_EQ(std::any_cast<int>(error), 123);
    }
}


TEST(Trampoline,AsyncAssignment) {
    auto op = TrampolineOp::async([](auto sched) {
        return Deferred<std::any,std::any>::pure(123);
    });
    TrampolineOp op2 = *op;
    EXPECT_EQ(op->opType, op2.opType);

    auto left = op->data.thunkData->target<DeferredRef<std::any,std::any>(*)(std::shared_ptr<Scheduler>)>();
    auto right = op2.data.thunkData->target<DeferredRef<std::any,std::any>(*)(std::shared_ptr<Scheduler>)>();
    EXPECT_EQ(left, right);
}

TEST(Trampoline,AsyncMove) {
    auto op = TrampolineOp::async([](auto sched) {
        return Deferred<std::any,std::any>::pure(123);
    });
    TrampolineOp op2 = *op;
    TrampolineOp op3 = std::move(op2);
    EXPECT_EQ(op->opType, op3.opType);

    auto left = op->data.thunkData->target<DeferredRef<std::any,std::any>(*)(std::shared_ptr<Scheduler>)>();
    auto right = op3.data.thunkData->target<DeferredRef<std::any,std::any>(*)(std::shared_ptr<Scheduler>)>();
    EXPECT_EQ(left, right);
}

TEST(Trampoline,FlatMapValue) {
    auto op = TrampolineOp::value(123)->flatMap([](auto erased_value, auto isError) {
        if(isError) {
            auto value = std::any_cast<int>(erased_value);
            return TrampolineOp::error(value * 1.5f);
        } else {
            auto value = std::any_cast<int>(erased_value);
            return TrampolineOp::value(value * 2.5f);
        }
    });
    auto result = TrampolineRunLoop::execute(op, Scheduler::global());
    auto syncResult = std::get<Either<std::any,std::any>>(result);
    auto value = std::any_cast<float>(syncResult.get_left());
    EXPECT_EQ(value, 307.5);
}

TEST(Trampoline,FlatMapError) {
    auto op = TrampolineOp::error(123)->flatMap([](auto erased_value, auto isError) {
        if(isError) {
            auto value = std::any_cast<int>(erased_value);
            return TrampolineOp::error(value * 1.5f);
        } else {
            auto value = std::any_cast<int>(erased_value);
            return TrampolineOp::value(value * 2.5f);
        }
    });
    auto result = TrampolineRunLoop::execute(op, Scheduler::global());
    auto syncResult = std::get<Either<std::any,std::any>>(result);
    auto value = std::any_cast<float>(syncResult.get_right());
    EXPECT_EQ(value, 184.5);
}

TEST(Trampoline,FlatMapAsyncValue) {
    auto op = TrampolineOp::async([](auto sched) {
        return Deferred<std::any,std::any>::pure(123);
    })
    ->flatMap([](auto erased_value, auto isError) {
        if(isError) {
            auto value = std::any_cast<int>(erased_value);
            return TrampolineOp::error(value * 1.5f);
        } else {
            auto value = std::any_cast<int>(erased_value);
            return TrampolineOp::value(value * 2.5f);
        }
    });

    auto result = TrampolineRunLoop::execute(op, Scheduler::global());
    auto asyncResult = std::get<DeferredRef<std::any,std::any>>(result);
    auto awaitResult = asyncResult->await();
    EXPECT_EQ(std::any_cast<float>(awaitResult), 307.5);
}

TEST(Trampoline,FlatMapAsyncError) {
    auto op = TrampolineOp::async([](auto sched) {
        return Deferred<std::any,std::any>::raiseError(123);
    })
    ->flatMap([](auto erased_value, auto isError) {
        if(isError) {
            auto value = std::any_cast<int>(erased_value);
            return TrampolineOp::error(value * 1.5f);
        } else {
            auto value = std::any_cast<int>(erased_value);
            return TrampolineOp::value(value * 2.5f);
        }
    });

    auto result = TrampolineRunLoop::execute(op, Scheduler::global());
    auto asyncResult = std::get<DeferredRef<std::any,std::any>>(result);

        try {
        asyncResult->await();
        FAIL() << "Exceted function to throw";
    } catch(std::any& error) {
        EXPECT_EQ(std::any_cast<float>(error), 184.5);
    }
}

TEST(Trampoline,FlatMapAsyncNestedAsync) {
    auto op = TrampolineOp::async([](auto sched) {
        return Deferred<std::any,std::any>::pure(123);
    })
    ->flatMap([](auto erased_value, auto isError) {
        auto value = std::any_cast<int>(erased_value);
        return TrampolineOp::async([value](auto) {
            return Deferred<std::any,std::any>::pure(value * 1.5f);
        });
    });

    auto result = TrampolineRunLoop::execute(op, Scheduler::global());
    auto asyncResult = std::get<DeferredRef<std::any,std::any>>(result);
    auto awaitResult = asyncResult->await();
    EXPECT_EQ(std::any_cast<float>(awaitResult), 184.5);
}

TEST(Trampoline,FlatMapAssignment) {
    auto op = TrampolineOp::value(123)->flatMap([](auto erased_value, auto isError) {
        return TrampolineOp::value(erased_value);
    });
    TrampolineOp op2 = *op;
    EXPECT_EQ(op->opType, op2.opType);

    auto leftFunc = op->data.flatMapData->second.target<std::shared_ptr<TrampolineOp>(*)(const std::any&, bool)>();
    auto rightFunc = op2.data.flatMapData->second.target<std::shared_ptr<TrampolineOp>(*)(const std::any&, bool)>();
    EXPECT_EQ(leftFunc, rightFunc);
}

TEST(Trampoline,FlatMapMove) {
    auto op = TrampolineOp::value(123)->flatMap([](auto erased_value, auto isError) {
        return TrampolineOp::value(erased_value);
    });
    TrampolineOp op2 = *op;
    TrampolineOp op3 = std::move(op2);
    EXPECT_EQ(op->opType, op3.opType);

    auto leftFunc = op->data.flatMapData->second.target<std::shared_ptr<TrampolineOp>(*)(const std::any&, bool)>();
    auto rightFunc = op3.data.flatMapData->second.target<std::shared_ptr<TrampolineOp>(*)(const std::any&, bool)>();
    EXPECT_EQ(leftFunc, rightFunc);
}
