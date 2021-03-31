//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_PROMISE_DEFERRED_H_
#define _CASK_PROMISE_DEFERRED_H_

#include <any>
#include <functional>
#include <optional>
#include <memory>
#include <mutex>
#include <variant>
#include <vector>
#include "../Scheduler.hpp"

namespace cask::deferred {

template <class T, class E>
class PromiseDeferred;


template <class T, class E = std::any>
class PromiseDeferred final : public Deferred<T,E> {
public:
    explicit PromiseDeferred(std::shared_ptr<Promise<T,E>> promise);
    void onComplete(std::function<void(Either<T,E>)> callback) override;
    void onSuccess(std::function<void(T)> callback) override;
    void onError(std::function<void(E)> callback) override;
    void onCancel(std::function<void()> callback) override;
    void cancel() override;
    T await() override;
    std::optional<Either<T,E>> get() override;

    std::shared_ptr<Promise<T,E>> promise;
private:
    std::shared_ptr<Scheduler> sched;
};

template <class T, class E>
PromiseDeferred<T,E>::PromiseDeferred(std::shared_ptr<Promise<T,E>> promise)
    : promise(promise)
    , sched(promise->sched)
{}

template <class T, class E>
void PromiseDeferred<T,E>::onComplete(std::function<void(Either<T,E>)> callback) {
    promise->onComplete(callback);
}

template <class T, class E>
void PromiseDeferred<T,E>::onSuccess(std::function<void(T)> callback) {
    promise->onComplete([callback](Either<T,E> value) {
        if(value.is_left()) {
            callback(value.get_left());
        }
    });
}

template <class T, class E>
void PromiseDeferred<T,E>::onError(std::function<void(E)> callback) {
    promise->onComplete([callback](Either<T,E> value) {
        if(value.is_right()) {
            callback(value.get_right());
        }
    });
}

template <class T, class E>
void PromiseDeferred<T,E>::onCancel(std::function<void()> callback) {
    promise->onCancel(callback);
}

template <class T, class E>
void PromiseDeferred<T,E>::cancel() {
    promise->cancel();
}

template <class T, class E>
T PromiseDeferred<T,E>::await() {
    bool canceled = false;
    std::optional<Either<T,E>> result = promise->get();

    if(!result.has_value()) {
        std::mutex mutex;
        mutex.lock();

        promise->onComplete([&mutex, &result](Either<T,E> newResult){
            result = newResult;
            mutex.unlock();
        });

        promise->onCancel([&mutex, &canceled](){
            canceled = true;
            mutex.unlock();
        });

        mutex.lock();
    }

    if(canceled) {
        throw std::runtime_error("Awaiting a promise which was canceled");
    } else if(result->is_left()) {
        return result->get_left();
    } else {
        throw result->get_right();
    }
}

template <class T, class E>
std::optional<Either<T,E>> PromiseDeferred<T,E>::get() {
    return promise->get();
}

}

#endif