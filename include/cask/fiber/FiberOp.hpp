//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_FIBER_OP_H_
#define _CASK_FIBER_OP_H_

#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <tuple>
#include "FiberValue.hpp"
#include "../None.hpp"
#include "../Either.hpp"
#include "../Erased.hpp"
#include "../Scheduler.hpp"

namespace cask {

template <class T, class E>
class Deferred;

template <class T, class E>
using DeferredRef = std::shared_ptr<Deferred<T,E>>;

}

namespace cask::fiber {

enum FiberOpType { ASYNC, VALUE, ERROR, FLATMAP, THUNK, DELAY, RACE, CANCEL };

/**
 * A `FiberOp` represents a trampolined and possibly asynchronous program
 * that can be executed via a `Fiber`. The operations are not
 * meant to be used directly but rather as an intermediate description of
 * execution for higher-order monads (such as `Task`).
 * 
 * This engine supports only a few operations - from which a large number
 * of composite operations can be described:
 * 
 *   1. `Value` represents a pure value which does not need to be computed.
 *   2. `Error` represents an errors which should halt execution.
 *   3. `Thunk` represents a lazily-evaluated method which returns a `Value`.
 *   4. `Async` represents an asynchronous operation.
 *   5. `FlatMap` represents a composite program which takes the results
 *      from one program (the input) and provides it to another program (
 *      the predicate) which returns a new and likely transformed result.
 *   6. `Delay` represents a timed delay after which a fiber should resume
 *      execution.
 *   7. `Race` represents the parallel execution of several operations of
 *      which the result is provided for the first operation which completes
 *      and all other operations are canceled.
 *   6. `Cancel` represents the cancelation of evaluation for the fiber.
 */
class FiberOp final : public std::enable_shared_from_this<FiberOp> {
public:
    using ConstantData = Either<Erased,Erased>;
    using AsyncData = std::function<DeferredRef<Erased,Erased>(const std::shared_ptr<Scheduler>&)>;
    using ThunkData = std::function<Erased()>;
    using FlatMapInput = std::shared_ptr<const FiberOp>;
    using FlatMapPredicate = std::function<std::shared_ptr<const FiberOp>(const FiberValue&)>;
    using FlatMapData = std::pair<FlatMapInput,FlatMapPredicate>;
    using DelayData = int64_t;
    using RaceData = std::vector<std::shared_ptr<const FiberOp>>;

    /**
     * The type of operation represented. Used for optimization of internal
     * run loop mechanisms.
     */
    FiberOpType opType;

    static std::shared_ptr<const FiberOp> value(const Erased& v) noexcept;
    static std::shared_ptr<const FiberOp> value(Erased&& v) noexcept;
    static std::shared_ptr<const FiberOp> error(const Erased& e) noexcept;
    static std::shared_ptr<const FiberOp> async(const std::function<DeferredRef<Erased,Erased>(const std::shared_ptr<Scheduler>&)>& predicate) noexcept;
    static std::shared_ptr<const FiberOp> thunk(const std::function<Erased()>& thunk) noexcept;
    static std::shared_ptr<const FiberOp> delay(int64_t delay_ms) noexcept;
    static std::shared_ptr<const FiberOp> race(const std::vector<std::shared_ptr<const FiberOp>>& race) noexcept;
    static std::shared_ptr<const FiberOp> race(std::vector<std::shared_ptr<const FiberOp>>&& race) noexcept;
    static std::shared_ptr<const FiberOp> cancel() noexcept;

    /**
     * Create a new operation which represents the flat map of this operation
     * via the given predicate. This is a convenience method which hides some
     * of the type erasure and other internal bits from users.
     * 
     * @param predicate The method which maps the input value to a new operation.
     * @return A new operation which transforms the intput to the given output operation.
     */
    std::shared_ptr<const FiberOp> flatMap(const FlatMapPredicate& predicate) const noexcept;

    /**
     * Construct a fiber op of the given type. Should not be called directly and instead
     * users should use the static construction methods provided.
     */
    explicit FiberOp(AsyncData* async) noexcept;
    explicit FiberOp(ConstantData* constant) noexcept;
    explicit FiberOp(ThunkData* thunk) noexcept;
    explicit FiberOp(FlatMapData* flatMap) noexcept;
    explicit FiberOp(DelayData* delay) noexcept;
    explicit FiberOp(RaceData* race) noexcept;
    explicit FiberOp(bool cancel_flag) noexcept;

    union {
        AsyncData* asyncData;
        ConstantData* constantData;
        ThunkData* thunkData;
        FlatMapData* flatMapData;
        DelayData* delayData;
        RaceData* raceData;
    } data;

    ~FiberOp();
};

} // namespace cask::fiber

#endif