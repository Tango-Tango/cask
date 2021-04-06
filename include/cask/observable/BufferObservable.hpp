//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_BUFFER_OBSERVABLE_
#define _CASK_BUFFER_OBSERVABLE_

#include "../Observable.hpp"
#include "../Observer.hpp"
#include "BufferObserver.hpp"

namespace cask::observable {

template <class T, class E>
class BufferObservable final : public Observable<BufferRef<T>,E> {
public:
    explicit BufferObservable(const std::shared_ptr<const Observable<T,E>>& upstream, uint32_t buffer_size);
    CancelableRef subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<BufferRef<T>,E>>& observer) const override;
private:
    std::shared_ptr<const Observable<T,E>> upstream;
    uint32_t buffer_size;
};

template <class T, class E>
BufferObservable<T,E>::BufferObservable(const std::shared_ptr<const Observable<T,E>>& upstream, uint32_t buffer_size)
    : upstream(upstream)
    , buffer_size(buffer_size)
{}

template <class T, class E>
CancelableRef BufferObservable<T,E>::subscribe(
    const std::shared_ptr<Scheduler>& sched,
    const std::shared_ptr<Observer<BufferRef<T>,E>>& observer) const
{
    auto bufferObserver = std::make_shared<BufferObserver<T,E>>(observer, buffer_size);
    return upstream->subscribe(sched, bufferObserver);
}

}

#endif
