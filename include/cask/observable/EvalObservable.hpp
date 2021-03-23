//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_EVAL_OBSERVABLE_H_
#define _CASK_EVAL_OBSERVABLE_H_

#include "../Observable.hpp"

namespace cask::observable {

template <class T, class E>
class EvalObservable final : public Observable<T,E> {
public:
    explicit EvalObservable(std::function<T()> predicate);
    CancelableRef subscribe(std::shared_ptr<Scheduler> sched, std::shared_ptr<Observer<T,E>> observer) const;

private:
    std::function<T()> predicate;
};

template <class T, class E>
EvalObservable<T,E>::EvalObservable(std::function<T()> predicate)
    : predicate(predicate)
{}

template <class T, class E>
CancelableRef EvalObservable<T,E>::subscribe(
    std::shared_ptr<Scheduler>,
    std::shared_ptr<Observer<T,E>> observer) const
{
    try {
        auto deferred = observer->onNext(predicate());
        deferred->onSuccess([observer](auto) {
            observer->onComplete();
        });

        return deferred;
    } catch(E& error) {
        observer->onError(error);
        return std::make_unique<IgnoreCancelation>();
    }
}

}

#endif