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
    BufferObserver(const std::shared_ptr<Observer<BufferRef<T>,E>>& downstream, uint32_t buffer_size);
    Task<Ack,None> onNext(const T& value) override;
    Task<None,None> onError(const E& error) override;
    Task<None,None> onComplete() override;
    Task<None,None> onCancel() override;
private:
    std::shared_ptr<Observer<BufferRef<T>,E>> downstream;
    uint32_t buffer_size;
    BufferRef<T> buffer;
};


template <class T, class E>
BufferObserver<T,E>::BufferObserver(const std::shared_ptr<Observer<BufferRef<T>,E>>& downstream, uint32_t buffer_size)
    : downstream(downstream)
    , buffer_size(buffer_size)
    , buffer(std::make_shared<std::vector<T>>())
{
    buffer->reserve(buffer_size);
}

template <class T, class E>
Task<Ack,None> BufferObserver<T,E>::onNext(const T& value) {
    buffer->emplace_back(value);
    if(buffer->size() == buffer_size) {
        auto downstreamTask = downstream->onNext(buffer);
        buffer = std::make_shared<std::vector<T>>();
        buffer->reserve(buffer_size);
        return downstreamTask;
    } else {
        return Task<Ack,None>::pure(Continue);
    }
}

template <class T, class E>
Task<None,None> BufferObserver<T,E>::onError(const E& error) {
    return downstream->onError(error);
}

template <class T, class E>
Task<None,None> BufferObserver<T,E>::onComplete() {
    if(buffer->size() != 0) {
        return downstream->onNext(buffer)
            .template flatMap<None>([downstream = downstream](auto) {
                return downstream->onComplete();
            });
    } else {
        return downstream->onComplete();
    }
}

template <class T, class E>
Task<None,None> BufferObserver<T,E>::onCancel() {
    return downstream->onCancel();
}

}

#endif