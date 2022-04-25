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
    void operator()(T* ptr) {
        global_pool().deallocate<T>(ptr);
    }
};

FiberOp::FiberOp(AsyncData* async) noexcept
    : opType(ASYNC)
{
    data.asyncData = async;
}

FiberOp::FiberOp(ConstantData* constant) noexcept
{
    if(constant->is_left()) {
        opType = VALUE;
    } else {
        opType = ERROR;
    }
    
    data.constantData = constant;
}

FiberOp::FiberOp(ThunkData* thunk) noexcept
    : opType(THUNK)
{
    data.thunkData = thunk;
}

FiberOp::FiberOp(FlatMapData* flatMap) noexcept
    : opType(FLATMAP)
{
    data.flatMapData = flatMap;
}

FiberOp::FiberOp(DelayData* delay) noexcept
    : opType(DELAY)
{
    data.delayData = delay;
}

FiberOp::FiberOp(RaceData* race) noexcept
    : opType(RACE)
{
    data.raceData = race;
}

FiberOp::FiberOp(bool) noexcept
    : opType(CANCEL)
{}


FiberOp::~FiberOp() {
    switch(opType) {
        case VALUE:
        case ERROR:
            global_pool().deallocate<ConstantData>(data.constantData);
        break;
        case THUNK:
            global_pool().deallocate<ThunkData>(data.thunkData);
        break;
        case ASYNC:
            global_pool().deallocate<AsyncData>(data.asyncData);
        break;
        case FLATMAP:
            global_pool().deallocate<FlatMapData>(data.flatMapData);
        break;
        case DELAY:
            global_pool().deallocate<DelayData>(data.delayData);
        break;
        case RACE:
            global_pool().deallocate<RaceData>(data.raceData);
        break;
        case CANCEL:
        break;
    }
}

std::shared_ptr<const FiberOp> FiberOp::value(const Erased& v) noexcept {
    auto constant = global_pool().allocate<ConstantData>(Either<Erased,Erased>::left(v));
    auto op = global_pool().allocate<FiberOp>(constant); 
    return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>());
}

std::shared_ptr<const FiberOp> FiberOp::value(Erased&& v) noexcept {
    auto constant = global_pool().allocate<ConstantData>(Either<Erased,Erased>::left(v));
    auto op = global_pool().allocate<FiberOp>(constant); 
    return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>());
}

std::shared_ptr<const FiberOp> FiberOp::error(const Erased& e) noexcept {
    auto constant = global_pool().allocate<ConstantData>(Either<Erased,Erased>::right(e));
    auto op = global_pool().allocate<FiberOp>(constant); 
    return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>());
}

std::shared_ptr<const FiberOp> FiberOp::async(const  std::function<DeferredRef<Erased,Erased>(const std::shared_ptr<Scheduler>&)>& predicate) noexcept {
    auto async_data = global_pool().allocate<AsyncData>(predicate);
    auto op = global_pool().allocate<FiberOp>(async_data); 
    return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>());
}

std::shared_ptr<const FiberOp> FiberOp::thunk(const std::function<Erased()>& thunk) noexcept {
    auto thunk_data = global_pool().allocate<ThunkData>(thunk);
    auto op = global_pool().allocate<FiberOp>(thunk_data); 
    return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>());
}

std::shared_ptr<const FiberOp> FiberOp::delay(int64_t delay_ms) noexcept {
    auto delay_data = global_pool().allocate<DelayData>(delay_ms);
    auto op = global_pool().allocate<FiberOp>(delay_data); 
    return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>());
}

std::shared_ptr<const FiberOp> FiberOp::race(const std::vector<std::shared_ptr<const FiberOp>>& race) noexcept {
    auto race_data = global_pool().allocate<RaceData>(race);
    auto op = global_pool().allocate<FiberOp>(race_data); 
    return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>());
}

std::shared_ptr<const FiberOp> FiberOp::race(std::vector<std::shared_ptr<const FiberOp>>&& race) noexcept {
    auto race_data = global_pool().allocate<RaceData>(std::move(race));
    auto op = global_pool().allocate<FiberOp>(race_data); 
    return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>());
}

std::shared_ptr<const FiberOp> FiberOp::cancel() noexcept {
    auto op = global_pool().allocate<FiberOp>(true); 
    return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>());
}

std::shared_ptr<const FiberOp> FiberOp::flatMap(const FlatMapPredicate& predicate) const noexcept {
    switch(opType) {
        case VALUE:
        case ERROR:
        case THUNK:
        case ASYNC:
        case DELAY:
        case RACE:
        case CANCEL:
        {
            auto data = global_pool().allocate<FlatMapData>(this->shared_from_this(), predicate);
            auto op = global_pool().allocate<FiberOp>(data); 
            return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>());
        }
        break;
        case FLATMAP:
        {
            FiberOp::FlatMapData* data = this->data.flatMapData;
            auto fixed = std::bind(FiberOp::fixed_predicate, data->second, predicate, std::placeholders::_1);
            auto flatMapData = global_pool().allocate<FlatMapData>(data->first, fixed);
            auto op = global_pool().allocate<FiberOp>(flatMapData); 
            return std::shared_ptr<FiberOp>(op, PoolDeleter<FiberOp>());
        }
        break;
    }
    __builtin_unreachable();
}

std::shared_ptr<const FiberOp> FiberOp::fixed_predicate(
    const FlatMapPredicate& input_predicate,
    const FlatMapPredicate& output_predicate,
    const FiberValue& value
) {
    return input_predicate(value)->flatMap(output_predicate);
}

} // namespace cask::fiber
