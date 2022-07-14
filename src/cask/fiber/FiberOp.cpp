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

std::shared_ptr<const FiberOp> FiberOp::value(const Erased& v) noexcept {
    auto pool = global_pool();
    auto constant = pool->allocate<ConstantData>(Either<Erased,Erased>::left(v));
    auto op = pool->allocate<FiberOp>(constant, pool); 
    return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>(pool));
}

std::shared_ptr<const FiberOp> FiberOp::value(Erased&& v) noexcept {
    auto pool = global_pool();
    auto constant = pool->allocate<ConstantData>(Either<Erased,Erased>::left(v));
    auto op = pool->allocate<FiberOp>(constant, pool); 
    return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>(pool));
}

std::shared_ptr<const FiberOp> FiberOp::error(const Erased& e) noexcept {
    auto pool = global_pool();
    auto constant = pool->allocate<ConstantData>(Either<Erased,Erased>::right(e));
    auto op = pool->allocate<FiberOp>(constant, pool); 
    return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>(pool));
}

std::shared_ptr<const FiberOp> FiberOp::async(const  std::function<DeferredRef<Erased,Erased>(const std::shared_ptr<Scheduler>&)>& predicate) noexcept {
    auto pool = global_pool();
    auto async_data = pool->allocate<AsyncData>(predicate);
    auto op = pool->allocate<FiberOp>(async_data, pool); 
    return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>(pool));
}


std::shared_ptr<const FiberOp> FiberOp::thunk(const std::function<Erased()>& thunk) noexcept {
    auto pool = global_pool();
    auto thunk_data = pool->allocate<ThunkData>(thunk);
    auto op = pool->allocate<FiberOp>(thunk_data, pool); 
    return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>(pool));
}

std::shared_ptr<const FiberOp> FiberOp::delay(int64_t delay_ms) noexcept {
    auto pool = global_pool();
    auto delay_data = pool->allocate<DelayData>(delay_ms);
    auto op = pool->allocate<FiberOp>(delay_data, pool); 
    return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>(pool));
}

std::shared_ptr<const FiberOp> FiberOp::race(const std::vector<std::shared_ptr<const FiberOp>>& race) noexcept {
    auto pool = global_pool();
    auto race_data = pool->allocate<RaceData>(race);
    auto op = pool->allocate<FiberOp>(race_data, pool); 
    return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>(pool));
}

std::shared_ptr<const FiberOp> FiberOp::race(std::vector<std::shared_ptr<const FiberOp>>&& race) noexcept {
    auto pool = global_pool();
    auto race_data = pool->allocate<RaceData>(std::move(race));
    auto op = pool->allocate<FiberOp>(race_data, pool); 
    return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>(pool));
}

std::shared_ptr<const FiberOp> FiberOp::cancel() noexcept {
    auto pool = global_pool();
    auto op = pool->allocate<FiberOp>(CANCEL); 
    return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>(pool));
}

std::shared_ptr<const FiberOp> FiberOp::cede() noexcept {
    auto pool = global_pool();
    auto op = pool->allocate<FiberOp>(CEDE); 
    return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>(pool));
}


std::shared_ptr<const FiberOp> FiberOp::flatMap(FlatMapPredicate&& predicate) const noexcept {
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
            auto pool = global_pool();
            auto data = pool->allocate<FlatMapData>(this->shared_from_this(), predicate);
            auto op = pool->allocate<FiberOp>(data, pool); 
            return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>(pool));
        }
        break;
        case FLATMAP:
        {
            FiberOp::FlatMapData* data = this->data.flatMapData;
            auto fixed = [input_predicate = data->second, output_predicate = predicate](FiberValue&& value) mutable {
                return FiberOp::fixed_predicate(std::move(input_predicate), std::move(output_predicate), std::forward<FiberValue>(value));
            };
            auto pool = global_pool();
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

std::shared_ptr<const FiberOp> FiberOp::fixed_predicate(
    FlatMapPredicate&& input_predicate,
    FlatMapPredicate&& output_predicate,
    FiberValue&& value
) {
    return input_predicate(std::forward<FiberValue>(value))->flatMap(std::forward<FlatMapPredicate>(output_predicate));
}

} // namespace cask::fiber
