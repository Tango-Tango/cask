//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_FIBER_H_
#define _CASK_FIBER_H_

#include <atomic>
#include "cask/FiberOp.hpp"

namespace cask {

enum FiberState { READY, RUNNING, WAITING, COMPLETED, CANCELED };

template <class T, class E>
class Fiber {
public:
    Fiber(const std::shared_ptr<const FiberOp>& op);

    bool resume();
    FiberState getState();
    std::optional<T> getValue();
    std::optional<E> getError();

    void asyncError(const Erased& error);
    void asyncSuccess(const Erased& value);
    void cancel();

private:
    std::shared_ptr<const FiberOp> op;
    Erased value;
    Erased error;
    FiberOp::FlatMapPredicate nextOp;
    std::atomic<FiberState> state;
};

template <class T, class E>
Fiber<T,E>::Fiber(const std::shared_ptr<const FiberOp>& op)
    : op(op)
    , value()
    , error()
    , nextOp()
    , state(READY)
{}

template <class T, class E>
bool Fiber<T,E>::resume() {
    FiberState expected = READY;

    if(!state.compare_exchange_strong(expected, RUNNING)) {
        return false;
    }

    while(true) {
        switch(op->opType) {
            case VALUE:
            {
                const FiberOp::ConstantData* data = op->data.constantData;
                value = data->get_left();
                error.reset();
                op = nullptr;
            }
            break;
            case ERROR:
            {
                const FiberOp::ConstantData* data = op->data.constantData;
                error = data->get_right();
                value.reset();
                op = nullptr;
            }
            break;
            case THUNK:
            {
                const FiberOp::ThunkData* thunk = op->data.thunkData;
                value = (*thunk)();
                error.reset();
                op = nullptr;
            }
            break;
            case ASYNC:
            {
                state.store(WAITING);
                return true;
            }
            break;
            case FLATMAP:
            {
                const FiberOp::FlatMapData* data = op->data.flatMapData;
                nextOp = data->second;
                op = data->first;
            }
            break;
        }

        if(!nextOp) {
            state.store(COMPLETED);
            return true;
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

template <class T, class E>
FiberState Fiber<T,E>::getState() {
    return state;
}

template <class T, class E>
std::optional<T> Fiber<T,E>::getValue() {
    if(state.load() == COMPLETED && value.has_value()) {
        return value.get<T>();
    } else {
        return {};
    }
}

template <class T, class E>
std::optional<E> Fiber<T,E>::getError() {
    if(state.load() == COMPLETED && error.has_value()) {
        return error.get<E>();
    } else {
        return {};
    }
}

template <class T, class E>
void Fiber<T,E>::asyncError(const Erased& error) {
    if(state.load() == WAITING) {
        this->error = error;
        this->value.reset();

        if(nextOp) {
            op = nextOp(error, true);
            nextOp = nullptr;
            state.store(READY);
        } else {
            state.store(COMPLETED);
        }
    }
}

template <class T, class E>
void Fiber<T,E>::asyncSuccess(const Erased& value) {
    if(state.load() == WAITING) {
        this->value = value;
        this->error.reset();

        if(nextOp) {
            op = nextOp(value, false);
            nextOp = nullptr;
            state.store(READY);
        } else {
            state.store(COMPLETED);
        }
    }
}

template <class T, class E>
void Fiber<T,E>::cancel() {
    state.store(CANCELED);
}

}

#endif