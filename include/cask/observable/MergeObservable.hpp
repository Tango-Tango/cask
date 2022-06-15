//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_MERGE_OBSERVABLE_H_
#define _CASK_MERGE_OBSERVABLE_H_

#include "../Observable.hpp"
#include "MergeObserver.hpp"

namespace cask::observable {

template <class T, class E>
class MergeObservable final : public Observable<T,E> {
public:
    MergeObservable(const ObservableConstRef<ObservableConstRef<T,E>,E>& upstream);
    FiberRef<None,None> subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,E>>& observer) const override;
private:
    ObservableConstRef<ObservableConstRef<T,E>,E> upstream;
};

template <class T, class E>
MergeObservable<T,E>::MergeObservable(const ObservableConstRef<ObservableConstRef<T,E>,E>& upstream)
    : upstream(upstream)
{}

template <class T, class E>
FiberRef<None,None> MergeObservable<T,E>::subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,E>>& observer) const {
    auto mergeObserver = std::make_shared<MergeObserver<T,E>>(observer, sched);
    return upstream->subscribe(sched, mergeObserver);
}

} // namespace cask::observable

#endif