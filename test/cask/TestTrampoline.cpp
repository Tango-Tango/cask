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
using cask::Erased;
using cask::Promise;
using cask::Scheduler;
using cask::trampoline::AsyncBoundary;
using cask::trampoline::TrampolineOp;
using cask::trampoline::TrampolineRunLoop;

TEST(Trampoline,Value) {
    auto op = TrampolineOp::value(123);
    auto result = TrampolineRunLoop::execute(op, Scheduler::global());
    auto syncResult = std::get<Either<Erased,Erased>>(result);
    auto value = syncResult.get_left().template get<int>();
    EXPECT_EQ(value, 123);
}

TEST(Trampoline,ValueAssignment) {
    auto op = TrampolineOp::value(123);
    TrampolineOp op2 = *op;
    EXPECT_EQ(op->opType, op2.opType);

    auto left = op->data.constantData->get_left();
    auto right = op2.data.constantData->get_left();
    EXPECT_EQ(left.template get<int>(), right.template get<int>());
}

TEST(Trampoline,ValueMove) {
    auto op = TrampolineOp::value(123);
    TrampolineOp op2 = *op;
    TrampolineOp op3 = std::move(op2);
    EXPECT_EQ(op->opType, op3.opType);

    auto left = op->data.constantData->get_left();
    auto right = op3.data.constantData->get_left();
    EXPECT_EQ(left.template get<int>(), right.template get<int>());
}

TEST(Trampoline,ValueSync) {
    auto op = TrampolineOp::value(123);
    auto result = TrampolineRunLoop::executeSync(op);
    auto syncResult = std::get<Either<Erased,Erased>>(result);
    auto value = syncResult.get_left().template get<int>();
    EXPECT_EQ(value, 123);
}

TEST(Trampoline,Error) {
    auto op = TrampolineOp::error(123);
    auto result = TrampolineRunLoop::execute(op, Scheduler::global());
    auto syncResult = std::get<Either<Erased,Erased>>(result);
    auto value = syncResult.get_right().template get<int>();
    EXPECT_EQ(value, 123);
}

TEST(Trampoline,ErrorAssignment) {
    auto op = TrampolineOp::error(123);
    TrampolineOp op2 = *op;
    EXPECT_EQ(op->opType, op2.opType);

    auto left = op->data.constantData->get_right();
    auto right = op2.data.constantData->get_right();
    EXPECT_EQ(left.template get<int>(), right.template get<int>());
}

TEST(Trampoline,ErrorMove) {
    auto op = TrampolineOp::error(123);
    TrampolineOp op2 = *op;
    TrampolineOp op3 = std::move(op2);
    EXPECT_EQ(op->opType, op3.opType);

    auto left = op->data.constantData->get_right();
    auto right = op3.data.constantData->get_right();
    EXPECT_EQ(left.template get<int>(), right.template get<int>());
}

TEST(Trampoline,ErrorSync) {
    auto op = TrampolineOp::error(123);
    auto result = TrampolineRunLoop::executeSync(op);
    auto syncResult = std::get<Either<Erased,Erased>>(result);
    auto value = syncResult.get_right().template get<int>();
    EXPECT_EQ(value, 123);
}

TEST(Trampoline,Thunk) {
    auto op = TrampolineOp::thunk([]() { return 123; });
    auto result = TrampolineRunLoop::execute(op, Scheduler::global());
    auto syncResult = std::get<Either<Erased,Erased>>(result);
    auto value = syncResult.get_left().template get<int>();
    EXPECT_EQ(value, 123);
}

TEST(Trampoline,ThunkAssignment) {
    auto op = TrampolineOp::thunk([]() { return 123; });
    TrampolineOp op2 = *op;
    EXPECT_EQ(op->opType, op2.opType);

    auto left = op->data.thunkData->target<Erased(*)()>();
    auto right = op2.data.thunkData->target<Erased(*)()>();
    EXPECT_EQ(left, right);
}

TEST(Trampoline,ThunkMove) {
    auto op = TrampolineOp::thunk([]() { return 123; });
    TrampolineOp op2 = *op;
    TrampolineOp op3 = std::move(op2);
    EXPECT_EQ(op->opType, op3.opType);

    auto left = op->data.thunkData->target<Erased(*)()>();
    auto right = op3.data.thunkData->target<Erased(*)()>();
    EXPECT_EQ(left, right);
}

TEST(Trampoline,ThunkSync) {
    auto op = TrampolineOp::thunk([]() { return 123; });
    auto result = TrampolineRunLoop::executeSync(op);
    auto syncResult = std::get<Either<Erased,Erased>>(result);
    auto value = syncResult.get_left().template get<int>();
    EXPECT_EQ(value, 123);
}

TEST(Trampoline,AsyncValue) {
    auto op = TrampolineOp::async([](auto) {
        return Deferred<Erased,Erased>::pure(123);
    });

    auto result = TrampolineRunLoop::execute(op, Scheduler::global());
    auto asyncResult = std::get<DeferredRef<Erased,Erased>>(result);
    auto awaitResult = asyncResult->await();
    auto value = awaitResult.template get<int>();
    EXPECT_EQ(value, 123);
}

TEST(Trampoline,AsyncValueSync) {
    auto op = TrampolineOp::async([](auto) {
        return Deferred<Erased,Erased>::pure(123);
    });

    auto result = TrampolineRunLoop::executeSync(op);
    auto boundary = std::get<AsyncBoundary>(result);

    auto asyncResult = TrampolineRunLoop::executeAsyncBoundary(boundary, Scheduler::global());
    auto awaitResult = asyncResult->await();
    auto value = awaitResult.template get<int>();
    EXPECT_EQ(value, 123);
}

TEST(Trampoline,AsyncError) {
    auto op = TrampolineOp::async([](auto) {
        return Deferred<Erased,Erased>::raiseError(123);
    });

    auto result = TrampolineRunLoop::execute(op, Scheduler::global());
    auto asyncResult = std::get<DeferredRef<Erased,Erased>>(result);

    try {
        asyncResult->await();
        FAIL() << "Exceted function to throw";
    } catch(Erased& error) {
        EXPECT_EQ(error.template get<int>(), 123);
    }
}

TEST(Trampoline,AsyncErrorSync) {
    auto op = TrampolineOp::async([](auto) {
        return Deferred<Erased,Erased>::raiseError(123);
    });

    auto result = TrampolineRunLoop::executeSync(op);
    auto boundary = std::get<AsyncBoundary>(result);
    auto asyncResult = TrampolineRunLoop::executeAsyncBoundary(boundary, Scheduler::global());

    try {
        asyncResult->await();
        FAIL() << "Exceted function to throw";
    } catch(Erased& error) {
        EXPECT_EQ(error.template get<int>(), 123);
    }
}

TEST(Trampoline,AsyncAssignment) {
    auto op = TrampolineOp::async([](auto) {
        return Deferred<Erased,Erased>::pure(123);
    });
    TrampolineOp op2 = *op;
    EXPECT_EQ(op->opType, op2.opType);

    auto left = op->data.thunkData->target<DeferredRef<Erased,Erased>(*)(std::shared_ptr<Scheduler>)>();
    auto right = op2.data.thunkData->target<DeferredRef<Erased,Erased>(*)(std::shared_ptr<Scheduler>)>();
    EXPECT_EQ(left, right);
}

TEST(Trampoline,AsyncMove) {
    auto op = TrampolineOp::async([](auto) {
        return Deferred<Erased,Erased>::pure(123);
    });
    TrampolineOp op2 = *op;
    TrampolineOp op3 = std::move(op2);
    EXPECT_EQ(op->opType, op3.opType);

    auto left = op->data.thunkData->target<DeferredRef<Erased,Erased>(*)(std::shared_ptr<Scheduler>)>();
    auto right = op3.data.thunkData->target<DeferredRef<Erased,Erased>(*)(std::shared_ptr<Scheduler>)>();
    EXPECT_EQ(left, right);
}

TEST(Trampoline,AsyncCancel) {
    auto op = TrampolineOp::async([](auto sched) {
        auto promise = Promise<Erased,Erased>::create(sched);
        return Deferred<Erased,Erased>::forPromise(promise);
    });

    auto result = TrampolineRunLoop::execute(op, Scheduler::global());
    auto asyncResult = std::get<DeferredRef<Erased,Erased>>(result);

    asyncResult->cancel();

    try {
        asyncResult->await();
        FAIL() << "Expected operation to throw";
    } catch(const std::runtime_error&) {}
}

TEST(Trampoline,AsyncCancelSync) {
    auto op = TrampolineOp::async([](auto sched) {
        auto promise = Promise<Erased,Erased>::create(sched);
        return Deferred<Erased,Erased>::forPromise(promise);
    });

    auto result = TrampolineRunLoop::executeSync(op);
    auto boundary = std::get<AsyncBoundary>(result);
    auto asyncResult = TrampolineRunLoop::executeAsyncBoundary(boundary, Scheduler::global());

    asyncResult->cancel();

    try {
        asyncResult->await();
        FAIL() << "Expected operation to throw";
    } catch(std::runtime_error&) {}
}

TEST(Trampoline,FlatMapValue) {
    auto op = TrampolineOp::value(123)->flatMap([](auto erased_value, auto isError) {
        if(isError) {
            auto value = erased_value.template get<int>();
            return TrampolineOp::error(value * 1.5f);
        } else {
            auto value = erased_value.template get<int>();
            return TrampolineOp::value(value * 2.5f);
        }
    });
    auto result = TrampolineRunLoop::execute(op, Scheduler::global());
    auto syncResult = std::get<Either<Erased,Erased>>(result);
    auto value = syncResult.get_left().template get<float>();
    EXPECT_EQ(value, 307.5);
}

TEST(Trampoline,FlatMapValueSync) {
    auto op = TrampolineOp::value(123)->flatMap([](auto erased_value, auto isError) {
        if(isError) {
            auto value = erased_value.template get<int>();
            return TrampolineOp::error(value * 1.5f);
        } else {
            auto value = erased_value.template get<int>();
            return TrampolineOp::value(value * 2.5f);
        }
    });
    auto result = TrampolineRunLoop::executeSync(op);
    auto syncResult = std::get<Either<Erased,Erased>>(result);
    auto value = syncResult.get_left().template get<float>();
    EXPECT_EQ(value, 307.5);
}

TEST(Trampoline,FlatMapError) {
    auto op = TrampolineOp::error(123)->flatMap([](auto erased_value, auto isError) {
        if(isError) {
            auto value = erased_value.template get<int>();
            return TrampolineOp::error(value * 1.5f);
        } else {
            auto value = erased_value.template get<int>();
            return TrampolineOp::value(value * 2.5f);
        }
    });
    auto result = TrampolineRunLoop::execute(op, Scheduler::global());
    auto syncResult = std::get<Either<Erased,Erased>>(result);
    auto value = syncResult.get_right().template get<float>();
    EXPECT_EQ(value, 184.5);
}

TEST(Trampoline,FlatMapErrorSync) {
    auto op = TrampolineOp::error(123)->flatMap([](auto erased_value, auto isError) {
        if(isError) {
            auto value = erased_value.template get<int>();
            return TrampolineOp::error(value * 1.5f);
        } else {
            auto value = erased_value.template get<int>();
            return TrampolineOp::value(value * 2.5f);
        }
    });
    auto result = TrampolineRunLoop::executeSync(op);
    auto syncResult = std::get<Either<Erased,Erased>>(result);
    auto value = syncResult.get_right().template get<float>();
    EXPECT_EQ(value, 184.5);
}

TEST(Trampoline,FlatMapAsyncValue) {
    auto op = TrampolineOp::async([](auto) {
        return Deferred<Erased,Erased>::pure(123);
    })
    ->flatMap([](auto erased_value, auto isError) {
        if(isError) {
            auto value = erased_value.template get<int>();
            return TrampolineOp::error(value * 1.5f);
        } else {
            auto value = erased_value.template get<int>();
            return TrampolineOp::value(value * 2.5f);
        }
    });

    auto result = TrampolineRunLoop::execute(op, Scheduler::global());
    auto asyncResult = std::get<DeferredRef<Erased,Erased>>(result);
    auto awaitResult = asyncResult->await();
    EXPECT_EQ(awaitResult.template get<float>(), 307.5);
}

TEST(Trampoline,FlatMapAsyncValueSync) {
    auto op = TrampolineOp::async([](auto) {
        return Deferred<Erased,Erased>::pure(123);
    })
    ->flatMap([](auto erased_value, auto isError) {
        if(isError) {
            auto value = erased_value.template get<int>();
            return TrampolineOp::error(value * 1.5f);
        } else {
            auto value = erased_value.template get<int>();
            return TrampolineOp::value(value * 2.5f);
        }
    });

    auto result = TrampolineRunLoop::executeSync(op);
    auto boundary = std::get<AsyncBoundary>(result);

    auto asyncResult = TrampolineRunLoop::executeAsyncBoundary(boundary, Scheduler::global());
    auto awaitResult = asyncResult->await();
    EXPECT_EQ(awaitResult.template get<float>(), 307.5);
}


TEST(Trampoline,FlatMapAsyncErrorSync) {
    auto op = TrampolineOp::async([](auto) {
        return Deferred<Erased,Erased>::raiseError(123);
    })
    ->flatMap([](auto erased_value, auto isError) {
        if(isError) {
            auto value = erased_value.template get<int>();
            return TrampolineOp::error(value * 1.5f);
        } else {
            auto value = erased_value.template get<int>();
            return TrampolineOp::value(value * 2.5f);
        }
    });

    auto result = TrampolineRunLoop::executeSync(op);
    auto boundary = std::get<AsyncBoundary>(result);
    auto asyncResult = TrampolineRunLoop::executeAsyncBoundary(boundary, Scheduler::global());

    try {
        asyncResult->await();
        FAIL() << "Exceted function to throw";
    } catch(Erased& error) {
        EXPECT_EQ(error.template get<float>(), 184.5);
    }
}

TEST(Trampoline,FlatMapAsyncNestedAsync) {
    auto op = TrampolineOp::async([](auto) {
        return Deferred<Erased,Erased>::pure(123);
    })
    ->flatMap([](auto erased_value, auto) {
        auto value = erased_value.template get<int>();
        return TrampolineOp::async([value](auto) {
            return Deferred<Erased,Erased>::pure(value * 1.5f);
        });
    });

    auto result = TrampolineRunLoop::execute(op, Scheduler::global());
    auto asyncResult = std::get<DeferredRef<Erased,Erased>>(result);
    auto awaitResult = asyncResult->await();
    EXPECT_EQ(awaitResult.template get<float>(), 184.5);
}

TEST(Trampoline,FlatMapAsyncNestedAsyncSync) {
    auto op = TrampolineOp::async([](auto) {
        return Deferred<Erased,Erased>::pure(123);
    })
    ->flatMap([](auto erased_value, auto) {
        auto value = erased_value.template get<int>();
        return TrampolineOp::async([value](auto) {
            return Deferred<Erased,Erased>::pure(value * 1.5f);
        });
    });

    auto result = TrampolineRunLoop::executeSync(op);
    auto boundary = std::get<AsyncBoundary>(result);
    auto asyncResult = TrampolineRunLoop::executeAsyncBoundary(boundary, Scheduler::global());

    auto awaitResult = asyncResult->await();
    EXPECT_EQ(awaitResult.template get<float>(), 184.5);
}

TEST(Trampoline,FlatMapAsyncCancel) {
    auto op = TrampolineOp::async([](auto sched) {
        auto promise = Promise<Erased,Erased>::create(sched);
        return Deferred<Erased,Erased>::forPromise(promise);
    })
    ->flatMap([](auto erased_value, auto isError) {
        if(isError) {
            return TrampolineOp::error(erased_value);
        } else {
            return TrampolineOp::value(erased_value);
        }
    });

    auto result = TrampolineRunLoop::execute(op, Scheduler::global());
    auto asyncResult = std::get<DeferredRef<Erased,Erased>>(result);

    asyncResult->cancel();

    try {
        asyncResult->await();
        FAIL() << "Expected test to throw an error";
    } catch(const std::runtime_error& ) {}
}

TEST(Trampoline,FlatMapAssignment) {
    auto op = TrampolineOp::value(123)->flatMap([](auto erased_value, auto) {
        return TrampolineOp::value(erased_value);
    });
    TrampolineOp op2 = *op;
    EXPECT_EQ(op->opType, op2.opType);

    auto leftFunc = op->data.flatMapData->second.target<std::shared_ptr<TrampolineOp>(*)(const Erased&, bool)>();
    auto rightFunc = op2.data.flatMapData->second.target<std::shared_ptr<TrampolineOp>(*)(const Erased&, bool)>();
    EXPECT_EQ(leftFunc, rightFunc);
}

TEST(Trampoline,FlatMapMove) {
    auto op = TrampolineOp::value(123)->flatMap([](auto erased_value, auto) {
        return TrampolineOp::value(erased_value);
    });
    TrampolineOp op2 = *op;
    TrampolineOp op3 = std::move(op2);
    EXPECT_EQ(op->opType, op3.opType);

    auto leftFunc = op->data.flatMapData->second.target<std::shared_ptr<TrampolineOp>(*)(const Erased&, bool)>();
    auto rightFunc = op3.data.flatMapData->second.target<std::shared_ptr<TrampolineOp>(*)(const Erased&, bool)>();
    EXPECT_EQ(leftFunc, rightFunc);
}
