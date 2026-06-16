//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_FIBER_OP_H_
#define _CASK_FIBER_OP_H_

#include <functional>
#include <atomic>
#include <cstdint>
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

class FiberOp;

/**
 * An intrusive reference-counted pointer to a `FiberOp`. FiberOps are
 * allocated from the memory pool and carry their own refcount, so no
 * separate `shared_ptr` control block is ever heap-allocated for them.
 * This keeps op creation on the pool fast-path - which matters because
 * op construction is one of the hottest allocation paths in the library.
 */
class FiberOpRef {
public:
    constexpr FiberOpRef() noexcept : ptr(nullptr) {}
    constexpr FiberOpRef(std::nullptr_t) noexcept : ptr(nullptr) {} // NOLINT(google-explicit-constructor)

    explicit FiberOpRef(const FiberOp* op) noexcept;

    FiberOpRef(const FiberOpRef& other) noexcept;
    FiberOpRef(FiberOpRef&& other) noexcept : ptr(other.ptr) {
        other.ptr = nullptr;
    }

    FiberOpRef& operator=(const FiberOpRef& other) noexcept;
    FiberOpRef& operator=(FiberOpRef&& other) noexcept;
    FiberOpRef& operator=(std::nullptr_t) noexcept;

    ~FiberOpRef();

    /**
     * Take ownership of an op whose refcount is already accounted for
     * (e.g. freshly constructed with a refcount of 1) without bumping
     * the count.
     */
    static FiberOpRef adopt(const FiberOp* op) noexcept {
        return FiberOpRef(op, AdoptTag{});
    }

    const FiberOp* get() const noexcept { return ptr; }
    const FiberOp& operator*() const noexcept { return *ptr; }
    const FiberOp* operator->() const noexcept { return ptr; }
    explicit operator bool() const noexcept { return ptr != nullptr; }

    bool operator==(const FiberOpRef& other) const noexcept { return ptr == other.ptr; }
    bool operator!=(const FiberOpRef& other) const noexcept { return ptr != other.ptr; }
    bool operator==(std::nullptr_t) const noexcept { return ptr == nullptr; }
    bool operator!=(std::nullptr_t) const noexcept { return ptr != nullptr; }

private:
    struct AdoptTag {};

    FiberOpRef(const FiberOp* op, AdoptTag) noexcept : ptr(op) {}

    const FiberOp* ptr;
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
class FiberOp final {
public:
    // ConstantData is now just Erased (48 bytes) instead of Either<Erased,Erased> (112 bytes).
    // The opType field (VALUE vs ERROR) already tells us which case it is.
    using ConstantData = Erased;
    using AsyncData = std::function<DeferredRef<Erased,Erased>(const std::shared_ptr<Scheduler>&)>;
    using ThunkData = std::function<Erased()>;
    using FlatMapInput = FiberOpRef;
    using FlatMapPredicate = std::function<FiberOpRef(FiberValue&&)>;
    using FlatMapData = std::pair<FlatMapInput,FlatMapPredicate>;
    using DelayData = int64_t;
    using RaceData = std::vector<FiberOpRef>;

    /**
     * The type of operation represented. Used for optimization of internal
     * run loop mechanisms.
     */
    FiberOpType opType;

    template <typename Arg>
    static FiberOpRef value(Arg&& value) noexcept  {
        auto pool = cask::pool::global_pool();
        auto constant = pool->allocate<ConstantData>(std::forward<Arg>(value));
        return FiberOpRef::adopt(pool->allocate<FiberOp>(constant, pool, VALUE));
    }

    template <typename Arg>
    static FiberOpRef error(Arg&& error) noexcept {
        auto pool = cask::pool::global_pool();
        auto constant = pool->allocate<ConstantData>(std::forward<Arg>(error));
        return FiberOpRef::adopt(pool->allocate<FiberOp>(constant, pool, ERROR));
    }

    template <typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<DeferredRef<Erased,Erased>(const std::shared_ptr<Scheduler>&)>
        >::value
    >>
    static FiberOpRef async(Predicate&& predicate) noexcept {
        auto pool = cask::pool::global_pool();
        auto async_data = pool->allocate<AsyncData>(std::forward<Predicate>(predicate));
        return FiberOpRef::adopt(pool->allocate<FiberOp>(async_data, pool));
    }

    template <typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<Erased()>
        >::value
    >>
    static FiberOpRef thunk(Predicate&& thunk) noexcept {
        auto pool = cask::pool::global_pool();
        auto thunk_data = pool->allocate<ThunkData>(std::forward<Predicate>(thunk));
        return FiberOpRef::adopt(pool->allocate<FiberOp>(thunk_data, pool));
    }

    static FiberOpRef delay(int64_t delay_ms) noexcept {
        auto pool = cask::pool::global_pool();
        auto delay_data = pool->allocate<DelayData>(delay_ms);
        return FiberOpRef::adopt(pool->allocate<FiberOp>(delay_data, pool));
    }

    template <typename Arg = std::vector<FiberOpRef>, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Arg>,
            std::vector<FiberOpRef>
        >::value
    >>
    static FiberOpRef race(Arg&& race) noexcept {
        auto pool = cask::pool::global_pool();
        auto race_data = pool->allocate<RaceData>(std::forward<Arg>(race));
        return FiberOpRef::adopt(pool->allocate<FiberOp>(race_data, pool));
    }

    static FiberOpRef cancel() noexcept {
        auto pool = cask::pool::global_pool();
        return FiberOpRef::adopt(pool->allocate<FiberOp>(CANCEL, pool));
    }

    static FiberOpRef cede() noexcept  {
        auto pool = cask::pool::global_pool();
        return FiberOpRef::adopt(pool->allocate<FiberOp>(CEDE, pool));
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
    FiberOpRef flatMap(Predicate&& predicate) const noexcept  {
        // Always create a new FLATMAP node wrapping this operation.
        // The continuation stack in FiberImpl handles chaining during evaluation,
        // so we don't need to create nested closures here.
        auto pool = cask::pool::global_pool();
        auto data = pool->allocate<FlatMapData>(FiberOpRef(this), std::forward<Predicate>(predicate));
        return FiberOpRef::adopt(pool->allocate<FiberOp>(data, pool));
    }

    /**
     * Check whether this op is uniquely referenced (refcount of exactly 1).
     * When the caller holds that single reference no other owner can
     * observe the op, so it may safely move payload data out of the op
     * rather than copying - the classic uniqueness optimization for
     * otherwise-immutable structures. Freshly built per-iteration ops
     * (e.g. the result of a flatMap continuation) are unique; ops still
     * referenced by a `Task` are not and must be copied from.
     */
    bool unique() const noexcept {
        return refcount.load(std::memory_order_acquire) == 1;
    }

    /**
     * Increment this op's intrusive reference count. Normally called only
     * by `FiberOpRef`.
     */
    void retain() const noexcept {
        refcount.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * Decrement this op's intrusive reference count - destroying the op and
     * returning its memory to the pool when the count reaches zero. Normally
     * called only by `FiberOpRef`.
     */
    void release() const noexcept {
        if (refcount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            auto* self = const_cast<FiberOp*>(this); // NOLINT(cppcoreguidelines-pro-type-const-cast)

            // Move the pool reference out of the object so it (and the pool
            // itself) reliably outlives the deallocation of this op. The
            // destructor sees a null pool and skips payload cleanup, so the
            // payload is released explicitly first.
            std::shared_ptr<Pool> local_pool = std::move(self->pool);
            self->deallocatePayload(*local_pool);
            local_pool->deallocate<FiberOp>(self);
        }
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
    explicit FiberOp(ConstantData* constant, const std::shared_ptr<Pool>& pool, FiberOpType type) noexcept;
    explicit FiberOp(ThunkData* thunk, const std::shared_ptr<Pool>& pool) noexcept;
    explicit FiberOp(FlatMapData* flatMap, const std::shared_ptr<Pool>& pool) noexcept;
    explicit FiberOp(DelayData* delay, const std::shared_ptr<Pool>& pool) noexcept;
    explicit FiberOp(RaceData* race, const std::shared_ptr<Pool>& pool) noexcept;
    explicit FiberOp(FiberOpType valueless_op, const std::shared_ptr<Pool>& pool) noexcept;

    ~FiberOp();
    
private:
    void deallocatePayload(Pool& pool) noexcept;

    mutable std::atomic<std::uint32_t> refcount;
    std::shared_ptr<Pool> pool;
};

inline FiberOpRef::FiberOpRef(const FiberOp* op) noexcept
    : ptr(op)
{
    if (ptr) {
        ptr->retain();
    }
}

inline FiberOpRef::FiberOpRef(const FiberOpRef& other) noexcept
    : ptr(other.ptr)
{
    if (ptr) {
        ptr->retain();
    }
}

inline FiberOpRef& FiberOpRef::operator=(const FiberOpRef& other) noexcept {
    if (this != &other) {
        const FiberOp* old = ptr;
        ptr = other.ptr;
        if (ptr) {
            ptr->retain();
        }
        if (old) {
            old->release();
        }
    }
    return *this;
}

inline FiberOpRef& FiberOpRef::operator=(FiberOpRef&& other) noexcept {
    if (this != &other) {
        const FiberOp* old = ptr;
        ptr = other.ptr;
        other.ptr = nullptr;
        if (old) {
            old->release();
        }
    }
    return *this;
}

inline FiberOpRef& FiberOpRef::operator=(std::nullptr_t) noexcept {
    const FiberOp* old = ptr;
    ptr = nullptr;
    if (old) {
        old->release();
    }
    return *this;
}

inline FiberOpRef::~FiberOpRef() {
    if (ptr) {
        ptr->release();
    }
}

} // namespace cask::fiber

#endif