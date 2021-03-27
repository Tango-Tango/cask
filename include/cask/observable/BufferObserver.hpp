//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_BUFFER_OBSERVER_H_
#define _CASK_BUFFER_OBSERVER_H_

#include "../BufferRef.hpp"
#include "../Observer.hpp"

namespace cask::observable {

/**
 * Represents an observer that transforms each event received on a stream to a new value and emits the
 * transformed event to a downstream observer. Normally obtained by calling `Observable<T>::map` and
 * then subscribring to the resulting observable.
 */
template <class T, class E>
class BufferObserver final : public Observer<T,E> {
public:
    BufferObserver(std::shared_ptr<Observer<BufferRef<T>,E>> downstream, unsigned int buffer_size);
    DeferredRef<Ack,E> onNext(T value) override;
    void onError(E error) override;
    void onComplete() override;
private:
    std::shared_ptr<Observer<BufferRef<T>,E>> downstream;
    unsigned int buffer_size;
    BufferRef<T> buffer;
};


template <class T, class E>
BufferObserver<T,E>::BufferObserver(std::shared_ptr<Observer<BufferRef<T>,E>> downstream, unsigned int buffer_size)
    : downstream(downstream)
    , buffer_size(buffer_size)
    , buffer(std::make_shared<std::vector<T>>())
{
    buffer->reserve(buffer_size);
}

template <class T, class E>
DeferredRef<Ack,E> BufferObserver<T,E>::onNext(T value) {
    buffer->emplace_back(value);
    if(buffer->size() == buffer_size) {
        auto deferred = downstream->onNext(buffer);
        buffer = std::make_shared<std::vector<T>>();
        buffer->reserve(buffer_size);
        return deferred;
    } else {
        return Deferred<Ack,E>::pure(Continue);
    }
}

template <class T, class E>
void BufferObserver<T,E>::onError(E error) {
    downstream->onError(error);
}

template <class T, class E>
void BufferObserver<T,E>::onComplete() {
    if(buffer->size() != 0) {
        downstream->onNext(buffer)->await();
    }
    downstream->onComplete();
}

}

#endif