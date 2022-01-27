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
    explicit EvalObservable(const std::function<T()>& predicate);
    FiberRef<None,None> subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,E>>& observer) const override;

private:
    std::function<T()> predicate;
};

template <class T, class E>
EvalObservable<T,E>::EvalObservable(const std::function<T()>& predicate)
    : predicate(predicate)
{}

template <class T, class E>
FiberRef<None,None> EvalObservable<T,E>::subscribe(
    const std::shared_ptr<Scheduler>& sched,
    const std::shared_ptr<Observer<T,E>>& observer) const
{
    try {
        return observer->onNext(predicate())
            .template flatMap<None>([observer](auto) {
                return observer->onComplete();
            })
            .doOnCancel(Task<None,None>::defer([observer] { return observer->onCancel(); }))
            .run(sched);
    } catch(E& error) {
        return observer->onError(error).run(sched);
    }
}

} // namespace cask::observable

#endif