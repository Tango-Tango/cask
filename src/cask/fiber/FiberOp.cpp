//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/Config.hpp"
#include "cask/Deferred.hpp"
#include "cask/fiber/FiberOp.hpp"
#include "cask/pool/InternalPool.hpp"
#include <utility>

using cask::pool::global_pool;

namespace cask::fiber {

FiberOp::FiberOp(AsyncData* async, const std::shared_ptr<Pool>& pool) noexcept
    : opType(ASYNC)
    , refcount(1)
    , pool(pool)
{
    data.asyncData = async;
}

FiberOp::FiberOp(ConstantData* constant, const std::shared_ptr<Pool>& pool, FiberOpType type) noexcept
    : opType(type)
    , refcount(1)
    , pool(pool)
{
    data.constantData = constant;
}

FiberOp::FiberOp(ThunkData* thunk, const std::shared_ptr<Pool>& pool) noexcept
    : opType(THUNK)
    , refcount(1)
    , pool(pool)
{
    data.thunkData = thunk;
}

FiberOp::FiberOp(FlatMapData* flatMap, const std::shared_ptr<Pool>& pool) noexcept
    : opType(FLATMAP)
    , refcount(1)
    , pool(pool)
{
    data.flatMapData = flatMap;
}

FiberOp::FiberOp(DelayData* delay, const std::shared_ptr<Pool>& pool) noexcept
    : opType(DELAY)
    , refcount(1)
    , pool(pool)
{
    data.delayData = delay;
}

FiberOp::FiberOp(RaceData* race, const std::shared_ptr<Pool>& pool) noexcept
    : opType(RACE)
    , refcount(1)
    , pool(pool)
{
    data.raceData = race;
}

FiberOp::FiberOp(FiberOpType valueless_op, const std::shared_ptr<Pool>& pool) noexcept
    : opType(valueless_op)
    , refcount(1)
    , pool(pool)
{}

void FiberOp::deallocatePayload(Pool& pool) noexcept {
    switch(opType) {
        case VALUE:
        case ERROR:
            pool.deallocate<ConstantData>(data.constantData);
        break;
        case THUNK:
            pool.deallocate<ThunkData>(data.thunkData);
        break;
        case ASYNC:
            pool.deallocate<AsyncData>(data.asyncData);
        break;
        case FLATMAP:
            pool.deallocate<FlatMapData>(data.flatMapData);
        break;
        case DELAY:
            pool.deallocate<DelayData>(data.delayData);
        break;
        case RACE:
            pool.deallocate<RaceData>(data.raceData);
        break;
        case CANCEL:
        case CEDE:
        break;
    }
}

FiberOp::~FiberOp() {
    // When destroyed via release() the pool member has already been moved
    // out (and the payload freed) - so only clean up the payload here when
    // the op is destroyed directly (e.g. by Pool::deallocate on an op which
    // was never wrapped in a FiberOpRef).
    if (pool) {
        deallocatePayload(*pool);
    }
}

} // namespace cask::fiber
