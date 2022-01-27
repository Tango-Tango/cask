//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_FILTER_OBSERVER_H_
#define _CASK_FILTER_OBSERVER_H_

#include "../Observer.hpp"

namespace cask::observable {

/**
 * Represents an observer that transforms each event received on a stream to a new value and emits the
 * transformed event to a downstream observer. Normally obtained by calling `Observable<T>::map` and
 * then subscribring to the resulting observable.
 */
template <class T, class E>
class FilterObserver final : public Observer<T,E> {
public:
    FilterObserver(const std::function<bool(const T&)>& predicate, const std::shared_ptr<Observer<T,E>>& downstream);
    Task<Ack,None> onNext(const T& value) override;
    Task<None,None> onError(const E& error) override;
    Task<None,None> onComplete() override;
    Task<None,None> onCancel() override;
private:
    std::function<bool(const T&)> predicate;
    std::shared_ptr<Observer<T,E>> downstream;
};


template <class T, class E>
FilterObserver<T,E>::FilterObserver(const std::function<bool(const T&)>& predicate, const std::shared_ptr<Observer<T,E>>& downstream)
    : predicate(predicate)
    , downstream(downstream)
{}

template <class T, class E>
Task<Ack,None> FilterObserver<T,E>::onNext(const T& value) {
    if(predicate(value)) {
        return downstream->onNext(value);
    } else {
        return Task<Ack,None>::pure(Continue);
    }
}

template <class T, class E>
Task<None,None> FilterObserver<T,E>::onError(const E& error) {
    return downstream->onError(error);
}

template <class T, class E>
Task<None,None> FilterObserver<T,E>::onComplete() {
    return downstream->onComplete();
}

template <class T, class E>
Task<None,None> FilterObserver<T,E>::onCancel() {
    return downstream->onCancel();
}

} // namespace cask::observable

#endif