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

template <class T>
class PoolDeleter {
public:
    explicit PoolDeleter(const std::shared_ptr<Pool>& pool)
        : pool(pool)
    {}

    void operator()(T* ptr) {
        pool->deallocate<T>(ptr);
    }

private:
    std::shared_ptr<Pool> pool;
};

enum FiberOpType : std::uint8_t { ASYNC, VALUE, ERROR, FLATMAP, THUNK, DELAY, RACE, CANCEL, CEDE };

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
    using FlatMapPredicate = std::function<std::shared_ptr<const FiberOp>(FiberValue&&)>;
    using FlatMapData = std::pair<FlatMapInput,FlatMapPredicate>;
    using DelayData = int64_t;
    using RaceData = std::vector<std::shared_ptr<const FiberOp>>;

    /**
     * The type of operation represented. Used for optimization of internal
     * run loop mechanisms.
     */
    FiberOpType opType;

    template <typename Arg>
    static std::shared_ptr<const FiberOp> value(Arg&& value) noexcept  {
        auto pool = cask::pool::global_pool();
        auto erased = Erased(std::forward<Arg>(value));
        auto constant = pool->allocate<ConstantData>(Either<Erased,Erased>::left(std::move(erased)));
        auto op = pool->allocate<FiberOp>(constant, pool); 
        return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>(pool));
    }

    template <typename Arg>
    static std::shared_ptr<const FiberOp> error(Arg&& error) noexcept {
        auto pool = cask::pool::global_pool();
        auto erased = Erased(std::forward<Arg>(error));
        auto constant = pool->allocate<ConstantData>(Either<Erased,Erased>::right(std::move(erased)));
        auto op = pool->allocate<FiberOp>(constant, pool); 
        return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>(pool));
    }

    template <typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<DeferredRef<Erased,Erased>(const std::shared_ptr<Scheduler>&)>
        >::value
    >>
    static std::shared_ptr<const FiberOp> async(Predicate&& predicate) noexcept {
        auto pool = cask::pool::global_pool();
        auto async_data = pool->allocate<AsyncData>(std::forward<Predicate>(predicate));
        auto op = pool->allocate<FiberOp>(async_data, pool); 
        return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>(pool));
    }

    template <typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<Erased()>
        >::value
    >>
    static std::shared_ptr<const FiberOp> thunk(Predicate&& thunk) noexcept {
        auto pool = cask::pool::global_pool();
        auto thunk_data = pool->allocate<ThunkData>(std::forward<Predicate>(thunk));
        auto op = pool->allocate<FiberOp>(thunk_data, pool); 
        return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>(pool));
    }

    static std::shared_ptr<const FiberOp> delay(int64_t delay_ms) noexcept {
        auto pool = cask::pool::global_pool();
        auto delay_data = pool->allocate<DelayData>(delay_ms);
        auto op = pool->allocate<FiberOp>(delay_data, pool); 
        return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>(pool));
    }

    template <typename Arg = std::vector<std::shared_ptr<const FiberOp>>, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Arg>,
            std::vector<std::shared_ptr<const FiberOp>>
        >::value
    >>
    static std::shared_ptr<const FiberOp> race(Arg&& race) noexcept {
        auto pool = cask::pool::global_pool();
        auto race_data = pool->allocate<RaceData>(std::forward<Arg>(race));
        auto op = pool->allocate<FiberOp>(race_data, pool); 
        return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>(pool));
    }

    static std::shared_ptr<const FiberOp> cancel() noexcept {
        auto pool = cask::pool::global_pool();
        auto op = pool->allocate<FiberOp>(CANCEL); 
        return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>(pool));
    }

    static std::shared_ptr<const FiberOp> cede() noexcept  {
        auto pool = cask::pool::global_pool();
        auto op = pool->allocate<FiberOp>(CEDE); 
        return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>(pool));
    }

    /**
     * Create a new operation which represents the flat map of this operation
     * via the given predicate. This is a convenience method which hides some
     * of the type erasure and other internal bits from users.
     * 
     * @param predicate The method which maps the input value to a new operation.
     * @return A new operation which transforms the intput to the given output operation.
     */
    template <typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            FlatMapPredicate
        >::value
    >>
    std::shared_ptr<const FiberOp> flatMap(Predicate&& predicate) const noexcept  {
        switch(opType) {
            case VALUE:
            case ERROR:
            case THUNK:
            case ASYNC:
            case DELAY:
            case RACE:
            case CANCEL:
            case CEDE:
            {
                auto pool = cask::pool::global_pool();
                auto data = pool->allocate<FlatMapData>(this->shared_from_this(), std::forward<Predicate>(predicate));
                auto op = pool->allocate<FiberOp>(data, pool); 
                return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>(pool));
            }
            break;
            case FLATMAP:
            {
                FiberOp::FlatMapData* data = this->data.flatMapData;
                auto fixed = [input_predicate = data->second, output_predicate = std::forward<Predicate>(predicate)](FiberValue&& value) mutable {
                    return input_predicate(std::forward<FiberValue>(value))->flatMap(std::forward<Predicate>(output_predicate));
                };
                auto pool = cask::pool::global_pool();
                auto flatMapData = pool->allocate<FlatMapData>(data->first, fixed);
                auto op = pool->allocate<FiberOp>(flatMapData, pool); 
                return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>(pool));
            }
            break;
        }
    #ifdef __GNUC__
        __builtin_unreachable();
    #endif
}

    union {
        AsyncData* asyncData;
        ConstantData* constantData;
        ThunkData* thunkData;
        FlatMapData* flatMapData;
        DelayData* delayData;
        RaceData* raceData;
    } data;

    /**
     * Construct a fiber op of the given type. Should not be called directly and instead
     * users should use the static construction methods provided.
     */
    explicit FiberOp(AsyncData* async, const std::shared_ptr<Pool>& pool) noexcept;
    explicit FiberOp(ConstantData* constant, const std::shared_ptr<Pool>& pool) noexcept;
    explicit FiberOp(ThunkData* thunk, const std::shared_ptr<Pool>& pool) noexcept;
    explicit FiberOp(FlatMapData* flatMap, const std::shared_ptr<Pool>& pool) noexcept;
    explicit FiberOp(DelayData* delay, const std::shared_ptr<Pool>& pool) noexcept;
    explicit FiberOp(RaceData* race, const std::shared_ptr<Pool>& pool) noexcept;
    explicit FiberOp(FiberOpType valueless_op) noexcept;    

    ~FiberOp();
    
private:
    std::shared_ptr<Pool> pool;
};

} // namespace cask::fiber

#endif