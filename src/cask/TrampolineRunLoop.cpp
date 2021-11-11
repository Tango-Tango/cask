//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/trampoline/TrampolineRunLoop.hpp"

#include <exception>

using cask::DeferredRef;

namespace cask::trampoline {

TrampolineResult TrampolineRunLoop::execute(
    const std::shared_ptr<const TrampolineOp>& initialOp,
    const std::shared_ptr<Scheduler>& sched
) {
    auto result = executeSync(initialOp);

    if(auto boundary = std::get_if<AsyncBoundary>(&result)) {
        return executeAsyncBoundary(*boundary, sched);
    } else {
        return std::get<Either<Erased,Erased>>(result);
    }
}

TrampolineSyncResult TrampolineRunLoop::executeSync(
    const std::shared_ptr<const TrampolineOp>& initialOp
) {
    std::shared_ptr<const TrampolineOp> op = initialOp;
    Erased value;
    Erased error;
    TrampolineOp::FlatMapPredicate nextOp;

    while(true) {
        switch(op->opType) {
            case VALUE:
            {
                const TrampolineOp::ConstantData* data = op->data.constantData;
                value = data->get_left();
                error.reset();
                op = nullptr;
            }
            break;
            case ERROR:
            {
                const TrampolineOp::ConstantData* data = op->data.constantData;
                error = data->get_right();
                value.reset();
                op = nullptr;
            }
            break;
            case THUNK:
            {
                const TrampolineOp::ThunkData* thunk = op->data.thunkData;
                value = (*thunk)();
                error.reset();
                op = nullptr;
            }
            break;
            case ASYNC:
            {
                return AsyncBoundary(op, nextOp);
            }
            break;
            case FLATMAP:
            {
                const TrampolineOp::FlatMapData* data = op->data.flatMapData;
                nextOp = data->second;
                op = data->first;
            }
            break;
        }

        if(!nextOp) {
            if(error.has_value()) {
                return Either<Erased,Erased>::right(error);
            } else {
                return Either<Erased,Erased>::left(value);
            }
        } else if(op == nullptr) {
            if(error.has_value()) {
                op = nextOp(error, true);
            } else {
                op = nextOp(value, false);
            }
            
            nextOp = nullptr;
        }
    }
}

DeferredRef<Erased,Erased> TrampolineRunLoop::executeAsyncBoundary(
    const AsyncBoundary& boundary,
    const std::shared_ptr<Scheduler>& sched
) {
    const std::shared_ptr<const TrampolineOp>& op = std::get<0>(boundary);
    const TrampolineOp::FlatMapPredicate& nextOp = std::get<1>(boundary);

    const TrampolineOp::AsyncData* async = op->data.asyncData;
    
    auto deferred = (*async)(sched);

    if(nextOp) {
        auto promise = Promise<Erased,Erased>::create(sched);

        deferred->template chainDownstreamAsync<Erased,Erased>(
            promise,
            [nextOp, sched](auto value) {
                Erased nextInput;

                if(value.is_left()) {
                    nextInput = value.get_left();
                } else {
                    nextInput = value.get_right();
                }

                auto result = TrampolineRunLoop::execute(nextOp(nextInput, value.is_right()), sched);

                if(auto syncResult = std::get_if<Either<Erased,Erased>>(&result)) {
                    if(syncResult->is_left()) {
                        return Deferred<Erased,Erased>::pure(syncResult->get_left());
                    } else {
                        return Deferred<Erased,Erased>::raiseError(syncResult->get_right());
                    }
                } else {
                    return std::get<DeferredRef<Erased,Erased>>(result);
                }
            }
        );

        return Deferred<Erased,Erased>::forPromise(promise);
    } else {
        return deferred;
    }
}

} // namespace cask::trampoline
