//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/FiberOp.hpp"
#include <utility>

namespace cask {

FiberOp::FiberOp(const FiberOp& other) noexcept
    : std::enable_shared_from_this<FiberOp>(other)
    , opType(other.opType)
{
    switch(opType) {
        case VALUE:
        case ERROR:
            data.constantData = new ConstantData(*(other.data.constantData));
        break;
        case THUNK:
            data.thunkData = new ThunkData(*(other.data.thunkData));
        break;
        case ASYNC:
            data.asyncData = new AsyncData(*(other.data.asyncData));
        break;
        case FLATMAP:
            data.flatMapData = new FlatMapData(*(other.data.flatMapData));
        break;
        case DELAY:
            data.delayData = new DelayData(*(other.data.delayData));
        break;
    }
}

FiberOp::FiberOp(FiberOp&& other) noexcept
    : opType(other.opType)
{
    switch(opType) {
        case VALUE:
        case ERROR:
            data.constantData = other.data.constantData;
            other.data.constantData = nullptr;
        break;
        case THUNK:
            data.thunkData = other.data.thunkData;
            other.data.thunkData = nullptr;
        break;
        case ASYNC:
            data.asyncData = other.data.asyncData;
            other.data.asyncData = nullptr;
        break;
        case FLATMAP:
            data.flatMapData = other.data.flatMapData;
            other.data.flatMapData = nullptr;
        break;
        case DELAY:
            data.delayData = other.data.delayData;
            other.data.delayData = nullptr;
        break;
    }
}

FiberOp::FiberOp(AsyncData* async) noexcept
    : opType(ASYNC)
{
    data.asyncData = async;
}

FiberOp::FiberOp(ConstantData* constant) noexcept
    : opType(VALUE)
{
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

FiberOp::~FiberOp() {
    switch(opType) {
        case VALUE:
        case ERROR:
            delete data.constantData;
        break;
        case THUNK:
            delete data.thunkData;
        break;
        case ASYNC:
            delete data.asyncData;
        break;
        case FLATMAP:
            delete data.flatMapData;
        break;
        case DELAY:
            delete data.delayData;
        break;
    }
}

FiberOp& FiberOp::operator=(const FiberOp& other) {
    this->opType = other.opType;
    this->data = other.data;
    return *this;
}

std::shared_ptr<const FiberOp> FiberOp::value(const Erased& v) noexcept {
    auto constant = new ConstantData(Either<Erased,Erased>::left(v));
    return std::make_shared<FiberOp>(constant);
}

std::shared_ptr<const FiberOp> FiberOp::value(Erased&& v) noexcept {
    auto constant = new ConstantData(Either<Erased,Erased>::left(v));
    return std::make_shared<FiberOp>(constant);
}

std::shared_ptr<const FiberOp> FiberOp::error(const Erased& e) noexcept {
    return std::make_shared<FiberOp>(new ConstantData(Either<Erased,Erased>::right(e)));
}

std::shared_ptr<const FiberOp> FiberOp::async(const  std::function<DeferredRef<Erased,Erased>(const std::shared_ptr<Scheduler>&)>& predicate) noexcept {
    return std::make_shared<FiberOp>(new AsyncData(predicate));
}

std::shared_ptr<const FiberOp> FiberOp::thunk(const std::function<Erased()>& thunk) noexcept {
    return std::make_shared<FiberOp>(new ThunkData(thunk));
}

std::shared_ptr<const FiberOp> FiberOp::delay(int64_t delay_ms) noexcept {
    return std::make_shared<FiberOp>(new DelayData(delay_ms));
}

std::shared_ptr<const FiberOp> FiberOp::flatMap(const FlatMapPredicate& predicate) const noexcept {
    switch(opType) {
        case VALUE:
        case ERROR:
        case THUNK:
        case ASYNC:
        case DELAY:
        {
            auto data = new FlatMapData(this->shared_from_this(), predicate);
            return std::make_shared<FiberOp>(data);
        }
        break;
        case FLATMAP:
        {
            FiberOp::FlatMapData* data = this->data.flatMapData;
            auto fixedPredicate = [inputPredicate = data->second, outputPredicate = predicate](auto value, auto isError) {
                return inputPredicate(value, isError)->flatMap(outputPredicate);
            };
            auto flatMapData = new FlatMapData(data->first, fixedPredicate);
            return std::make_shared<FiberOp>(flatMapData);
        }
        break;
    }
    __builtin_unreachable();
}

} // namespace cask::trampoline
