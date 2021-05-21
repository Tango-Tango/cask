//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/trampoline/TrampolineOp.hpp"
#include <utility>

namespace cask::trampoline {

TrampolineOp::TrampolineOp(const TrampolineOp& other) noexcept
    : std::enable_shared_from_this<TrampolineOp>(other)
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
    }
}

TrampolineOp::TrampolineOp(TrampolineOp&& other) noexcept
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
    }
}

TrampolineOp::TrampolineOp(OpType opType, AsyncData* async) noexcept
    : opType(opType)
{
    data.asyncData = async;
}

TrampolineOp::TrampolineOp(OpType opType, ConstantData* constant) noexcept
    : opType(opType)
{
    data.constantData = constant;
}

TrampolineOp::TrampolineOp(OpType opType, ThunkData* thunk) noexcept
    : opType(opType)
{
    data.thunkData = thunk;
}

TrampolineOp::TrampolineOp(OpType opType, FlatMapData* flatMap) noexcept
    : opType(opType)
{
    data.flatMapData = flatMap;
}

TrampolineOp::~TrampolineOp() {
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
    }
}

TrampolineOp& TrampolineOp::operator=(const TrampolineOp& other) {
    this->opType = other.opType;
    this->data = other.data;
    return *this;
}

std::shared_ptr<const TrampolineOp> TrampolineOp::value(const Erased& v) noexcept {
    auto constant = new ConstantData(Either<Erased,Erased>::left(v));
    return std::make_shared<TrampolineOp>(VALUE, constant);
}

std::shared_ptr<const TrampolineOp> TrampolineOp::value(Erased&& v) noexcept {
    auto constant = new ConstantData(Either<Erased,Erased>::left(v));
    return std::make_shared<TrampolineOp>(VALUE, constant);
}

std::shared_ptr<const TrampolineOp> TrampolineOp::error(const Erased& e) noexcept {
    return std::make_shared<TrampolineOp>(ERROR, new ConstantData(Either<Erased,Erased>::right(e)));
}

std::shared_ptr<const TrampolineOp> TrampolineOp::async(const std::function<DeferredRef<Erased,Erased>(std::shared_ptr<Scheduler>)>& predicate) noexcept {
    return std::make_shared<TrampolineOp>(ASYNC, new AsyncData(predicate));
}

std::shared_ptr<const TrampolineOp> TrampolineOp::thunk(const std::function<Erased()>& thunk) noexcept {
    return std::make_shared<TrampolineOp>(THUNK, new ThunkData(thunk));
}

std::shared_ptr<const TrampolineOp> TrampolineOp::flatMap(const FlatMapPredicate& predicate) const noexcept {
    switch(opType) {
        case VALUE:
        case ERROR:
        case THUNK:
        case ASYNC:
        {
            auto data = new FlatMapData(this->shared_from_this(), predicate);
            return std::make_shared<TrampolineOp>(FLATMAP, data);
        }
        break;
        case FLATMAP:
        {
            TrampolineOp::FlatMapData* data = this->data.flatMapData;
            auto fixedPredicate = [inputPredicate = data->second, outputPredicate = predicate](auto value, auto isError) {
                return inputPredicate(value, isError)->flatMap(outputPredicate);
            };
            auto flatMapData = new FlatMapData(data->first, fixedPredicate);
            return std::make_shared<TrampolineOp>(FLATMAP, flatMapData);
        }
        break;
    }
    __builtin_unreachable();
}

} // namespace cask::trampoline
