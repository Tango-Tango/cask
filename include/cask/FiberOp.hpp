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
#include "None.hpp"
#include "Either.hpp"
#include "Erased.hpp"

namespace cask {

enum FiberOpType { ASYNC, VALUE, ERROR, FLATMAP, THUNK};

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
 *   4. `Defer` represents an asynchronous operation.
 *   5. `FlatMap` represents a composite program which takes the results
 *      from one program (the input) and provides it to another program (
 *      the predicate) which returns a new and likely transformed result.
 */
class FiberOp : public std::enable_shared_from_this<FiberOp> {
public:
    using ConstantData = Either<Erased,Erased>;
    using AsyncData = cask::None;
    using ThunkData = std::function<Erased()>;
    using FlatMapInput = std::shared_ptr<const FiberOp>;
    using FlatMapPredicate = std::function<std::shared_ptr<const FiberOp>(const Erased&, bool)>;
    using FlatMapData = std::pair<FlatMapInput,FlatMapPredicate>; 

    /**
     * The type of operation represented. Used for optimization of internal
     * run loop mechanisms.
     */
    FiberOpType opType;

    static std::shared_ptr<const FiberOp> value(const Erased& v) noexcept;
    static std::shared_ptr<const FiberOp> value(Erased&& v) noexcept;
    static std::shared_ptr<const FiberOp> error(const Erased& e) noexcept;
    static std::shared_ptr<const FiberOp> async(const None& predicate) noexcept;
    static std::shared_ptr<const FiberOp> thunk(const std::function<Erased()>& thunk) noexcept;

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
     * Construct a trampoline op of the given type. Should be called by
     * subsclasses to instantiate the proper operation type.
     */
    FiberOp(const FiberOp& other) noexcept;
    FiberOp(FiberOp&& other) noexcept;
    FiberOp(AsyncData* async) noexcept;
    FiberOp(ConstantData* constant) noexcept;
    FiberOp(ThunkData* thunk) noexcept;
    FiberOp(FlatMapData* flatMap) noexcept;


    FiberOp& operator=(const FiberOp&);

    union {
        AsyncData* asyncData;
        ConstantData* constantData;
        ThunkData* thunkData;
        FlatMapData* flatMapData;
    } data;

    ~FiberOp();
};

}

#endif