//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_FILTER_OBSERVABLE_H_
#define _CASK_FILTER_OBSERVABLE_H_

#include "../Observable.hpp"
#include "FilterObserver.hpp"

namespace cask::observable {

/**
 * Represents an observable that transforms each element from an upstream observable
 * using the given predicate function. Normally obtained by calling `Observable<T>::map`.
 */
template <class T, class E>
class FilterObservable final : public Observable<T,E> {
public:
    FilterObservable(std::shared_ptr<const Observable<T,E>> upstream, std::function<bool(T)> predicate);
    CancelableRef subscribe(std::shared_ptr<Scheduler> sched, std::shared_ptr<Observer<T,E>> observer) const;
private:
    std::shared_ptr<const Observable<T,E>> upstream;
    std::function<bool(T)> predicate;
};

template <class T, class E>
FilterObservable<T,E>::FilterObservable(std::shared_ptr<const Observable<T,E>> upstream, std::function<bool(T)> predicate)
    : upstream(upstream)
    , predicate(predicate)
{}

template <class T, class E>
CancelableRef FilterObservable<T,E>::subscribe(std::shared_ptr<Scheduler> sched, std::shared_ptr<Observer<T,E>> observer) const {
    auto filterObserver = std::make_shared<FilterObserver<T,E>>(predicate, observer);
    return upstream->subscribe(sched, filterObserver);
}

}

#endif