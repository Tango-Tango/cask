//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_MAP_ERROR_OBSERVER_H_
#define _CASK_MAP_ERROR_OBSERVER_H_

#include "../Observer.hpp"

namespace cask::observable {

/**
 * Represents an observer that transforms each error received on a stream to a new value and emits the
 * transformed error to a downstream observer. Normally obtained by calling `Observable<T>::mapError` and
 * then subscribing to the resulting observable.
 */
template <class T, class EI, class EO>
class MapErrorObserver final : public Observer<T,EI> {
public:
    MapErrorObserver(const std::function<EO(const EI&)>& predicate, const std::shared_ptr<Observer<T,EO>>& downstream);
    Task<Ack,None> onNext(const T& value) override;
    Task<None,None> onError(const EI& error) override;
    Task<None,None> onComplete() override;
private:
    std::function<EO(EI)> predicate;
    std::shared_ptr<Observer<T,EO>> downstream;
};


template <class T, class EI, class EO>
MapErrorObserver<T,EI,EO>::MapErrorObserver(const std::function<EO(const EI&)>& predicate, const std::shared_ptr<Observer<T,EO>>& downstream)
    : predicate(predicate)
    , downstream(downstream)
{}

template <class T, class EI, class EO>
Task<Ack,None> MapErrorObserver<T,EI,EO>::onNext(const T& value) {
    return downstream->onNext(value);
}

template <class T, class EI, class EO>
Task<None,None> MapErrorObserver<T,EI,EO>::onError(const EI& error) {
    EO transformed = predicate(error);
    return downstream->onError(transformed);
}

template <class T, class EI, class EO>
Task<None,None> MapErrorObserver<T,EI,EO>::onComplete() {
    return downstream->onComplete();
}

}

#endif
