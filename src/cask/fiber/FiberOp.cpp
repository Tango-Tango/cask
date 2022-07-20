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
    , pool(pool)
{
    data.asyncData = async;
}

FiberOp::FiberOp(ConstantData* constant, const std::shared_ptr<Pool>& pool) noexcept
{
    if(constant->is_left()) {
        opType = VALUE;
    } else {
        opType = ERROR;
    }
    
    this->pool = pool;
    data.constantData = constant;
}

FiberOp::FiberOp(ThunkData* thunk, const std::shared_ptr<Pool>& pool) noexcept
    : opType(THUNK)
    , pool(pool)
{
    data.thunkData = thunk;
}

FiberOp::FiberOp(FlatMapData* flatMap, const std::shared_ptr<Pool>& pool) noexcept
    : opType(FLATMAP)
    , pool(pool)
{
    data.flatMapData = flatMap;
}

FiberOp::FiberOp(DelayData* delay, const std::shared_ptr<Pool>& pool) noexcept
    : opType(DELAY)
    , pool(pool)
{
    data.delayData = delay;
}

FiberOp::FiberOp(RaceData* race, const std::shared_ptr<Pool>& pool) noexcept
    : opType(RACE)
    , pool(pool)
{
    data.raceData = race;
}

FiberOp::FiberOp(FiberOpType valueless_op) noexcept
    : opType(valueless_op)
    , pool()
{}


FiberOp::~FiberOp() {
    switch(opType) {
        case VALUE:
        case ERROR:
            pool->deallocate<ConstantData>(data.constantData);
        break;
        case THUNK:
            pool->deallocate<ThunkData>(data.thunkData);
        break;
        case ASYNC:
            pool->deallocate<AsyncData>(data.asyncData);
        break;
        case FLATMAP:
            pool->deallocate<FlatMapData>(data.flatMapData);
        break;
        case DELAY:
            pool->deallocate<DelayData>(data.delayData);
        break;
        case RACE:
            pool->deallocate<RaceData>(data.raceData);
        break;
        case CANCEL:
        case CEDE:
        break;
    }
}

} // namespace cask::fiber
