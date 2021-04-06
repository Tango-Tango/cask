//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_TRAMPOLINE_OP_H_
#define _CASK_TRAMPOLINE_OP_H_

#include <any>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <tuple>
#include "../Deferred.hpp"
#include "../None.hpp"
#include "../Either.hpp"

namespace cask::trampoline {

enum OpType { ASYNC, VALUE, ERROR, FLATMAP, THUNK};

/**
 * A `TrampolineOp` represents a trampolined and possibly asynchronous program
 * that can be executed via the `TrampolineRunLoop`. The operations are not
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
class TrampolineOp : public std::enable_shared_from_this<TrampolineOp> {
public:
    using ConstantData = Either<std::any,std::any>;
    using AsyncData = std::function<DeferredRef<std::any,std::any>(const std::shared_ptr<Scheduler>&)>;
    using ThunkData = std::function<std::any()>;
    using FlatMapInput = std::shared_ptr<TrampolineOp>;
    using FlatMapPredicate = std::function<std::shared_ptr<TrampolineOp>(const std::any&, bool)>;
    using FlatMapData = std::pair<FlatMapInput,FlatMapPredicate>; 

    /**
     * The type of operation represented. Used for optimization of internal
     * run loop mechanisms.
     */
    OpType opType;

    static std::shared_ptr<TrampolineOp> value(const std::any& v) noexcept;
    static std::shared_ptr<TrampolineOp> value(std::any&& v) noexcept;
    static std::shared_ptr<TrampolineOp> error(const std::any& e) noexcept;
    static std::shared_ptr<TrampolineOp> async(const std::function<DeferredRef<std::any,std::any>(std::shared_ptr<Scheduler>)>& predicate) noexcept;
    static std::shared_ptr<TrampolineOp> thunk(const std::function<std::any()>& thunk) noexcept;

    /**
     * Create a new operation which represents the flat map of this operation
     * via the given predicate. This is a convenience method which hides some
     * of the type erasure and other internal bits from users.
     * 
     * @param predicate The method which maps the input value to a new operation.
     * @return A new operation which transforms the intput to the given output operation.
     */
    std::shared_ptr<TrampolineOp> flatMap(const FlatMapPredicate& predicate) noexcept;

    /**
     * Construct a trampoline op of the given type. Should be called by
     * subsclasses to instantiate the proper operation type.
     */
    TrampolineOp(const TrampolineOp& other) noexcept;
    TrampolineOp(TrampolineOp&& other) noexcept;
    TrampolineOp(OpType opType, AsyncData* async) noexcept;
    TrampolineOp(OpType opType, ConstantData* constant) noexcept;
    TrampolineOp(OpType opType, ThunkData* thunk) noexcept;
    TrampolineOp(OpType opType, FlatMapData* flatMap) noexcept;


    TrampolineOp& operator=(const TrampolineOp&);

    union {
        AsyncData* asyncData;
        ConstantData* constantData;
        ThunkData* thunkData;
        FlatMapData* flatMapData;
    } data;

    ~TrampolineOp();
};

}

#endif