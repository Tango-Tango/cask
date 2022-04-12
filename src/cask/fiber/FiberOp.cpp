//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/Deferred.hpp"
#include "cask/fiber/FiberOp.hpp"
#include <utility>

namespace cask::fiber {

Pool<128> FiberOp::pool(1024);

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
            FiberOp::pool.deallocate<ConstantData>(data.constantData);
        break;
        case THUNK:
            FiberOp::pool.deallocate<ThunkData>(data.thunkData);
        break;
        case ASYNC:
            FiberOp::pool.deallocate<AsyncData>(data.asyncData);
        break;
        case FLATMAP:
            FiberOp::pool.deallocate<FlatMapData>(data.flatMapData);
        break;
        case DELAY:
            FiberOp::pool.deallocate<DelayData>(data.delayData);
        break;
        case RACE:
            FiberOp::pool.deallocate<RaceData>(data.raceData);
        break;
        case CANCEL:
        break;
    }
}

std::shared_ptr<const FiberOp> FiberOp::value(const Erased& v) noexcept {
    auto constant = FiberOp::pool.allocate<ConstantData>(Either<Erased,Erased>::left(v));
    return std::make_shared<FiberOp>(constant);
}

std::shared_ptr<const FiberOp> FiberOp::value(Erased&& v) noexcept {
    auto constant = FiberOp::pool.allocate<ConstantData>(Either<Erased,Erased>::left(v));
    return std::make_shared<FiberOp>(constant);
}

std::shared_ptr<const FiberOp> FiberOp::error(const Erased& e) noexcept {
    return std::make_shared<FiberOp>(FiberOp::pool.allocate<ConstantData>(Either<Erased,Erased>::right(e)));
}

std::shared_ptr<const FiberOp> FiberOp::async(const  std::function<DeferredRef<Erased,Erased>(const std::shared_ptr<Scheduler>&)>& predicate) noexcept {
    return std::make_shared<FiberOp>(FiberOp::pool.allocate<AsyncData>(predicate));
}

std::shared_ptr<const FiberOp> FiberOp::thunk(const std::function<Erased()>& thunk) noexcept {
    return std::make_shared<FiberOp>(FiberOp::pool.allocate<ThunkData>(thunk));
}

std::shared_ptr<const FiberOp> FiberOp::delay(int64_t delay_ms) noexcept {
    return std::make_shared<FiberOp>(FiberOp::pool.allocate<DelayData>(delay_ms));
}

std::shared_ptr<const FiberOp> FiberOp::race(const std::vector<std::shared_ptr<const FiberOp>>& race) noexcept {
    return std::make_shared<FiberOp>(FiberOp::pool.allocate<RaceData>(race));
}

std::shared_ptr<const FiberOp> FiberOp::race(std::vector<std::shared_ptr<const FiberOp>>&& race) noexcept {
    return std::make_shared<FiberOp>(FiberOp::pool.allocate<RaceData>(std::move(race)));
}

std::shared_ptr<const FiberOp> FiberOp::cancel() noexcept {
    return std::make_shared<FiberOp>(true);
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
            auto data = FiberOp::pool.allocate<FlatMapData>(this->shared_from_this(), predicate);
            return std::make_shared<FiberOp>(data);
        }
        break;
        case FLATMAP:
        {
            FiberOp::FlatMapData* data = this->data.flatMapData;
            auto fixedPredicate = [inputPredicate = data->second, outputPredicate = predicate](auto value) {
                return inputPredicate(value)->flatMap(outputPredicate);
            };
            auto flatMapData = FiberOp::pool.allocate<FlatMapData>(data->first, fixedPredicate);
            return std::make_shared<FiberOp>(flatMapData);
        }
        break;
    }
    __builtin_unreachable();
}

} // namespace cask::fiber
