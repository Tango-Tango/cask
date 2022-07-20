//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)


#ifndef _CASK_DISTINCT_UNTIL_CHANGED_OBSERVER_H_
#define _CASK_DISTINCT_UNTIL_CHANGED_OBSERVER_H_

#include "../Observer.hpp"

namespace cask::observable {

/**
 * Implements an observer that suppresses consecutive events in the stream that
 * are the same - emitting downstream only the first.
 */
template <class T, class E>
class DistinctUntilChangedObserver final : public Observer<T,E> {
public:
    explicit DistinctUntilChangedObserver(const std::shared_ptr<Observer<T,E>>& downstream, const std::function<bool(const T&, const T&)>& comparator);

    Task<Ack,None> onNext(T&& value) override;
    Task<None,None> onError(E&& error) override;
    Task<None,None> onComplete() override;
    Task<None,None> onCancel() override;
private:
    std::shared_ptr<Observer<T,E>> downstream;
    std::function<bool(const T&, const T&)> comparator;
    std::optional<T> previous_value;
};

template <class T, class E>
DistinctUntilChangedObserver<T,E>::DistinctUntilChangedObserver(const std::shared_ptr<Observer<T,E>>& downstream, const std::function<bool(const T&, const T&)>& comparator)
    : downstream(downstream)
    , comparator(comparator)
    , previous_value()
{}

template <class T, class E>
Task<Ack, None> DistinctUntilChangedObserver<T,E>::onNext(T&& value) {
    if(previous_value.has_value() && comparator(*previous_value, value)) {
        return Task<Ack,None>::pure(Continue);
    } else {
        previous_value = value;
        return downstream->onNext(std::forward<T>(value));
    }
}

template <class T, class E>
Task<None,None> DistinctUntilChangedObserver<T,E>::onError(E&& error) {
    return downstream->onError(std::forward<E>(error));
}

template <class T, class E>
Task<None,None>  DistinctUntilChangedObserver<T,E>::onComplete() {
    return downstream->onComplete();
}

template <class T, class E>
Task<None,None>  DistinctUntilChangedObserver<T,E>::onCancel() {
    return downstream->onCancel();
}

} // namespace cask::observable

#endif