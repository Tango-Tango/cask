//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_VECTOR_OBSERVABLE_
#define _CASK_VECTOR_OBSERVABLE_

#include "../Observable.hpp"
#include "../Observer.hpp"

using namespace std::placeholders;

namespace cask::observable {

template <class T, class E>
class VectorObservable final : public Observable<T,E> {
public:
    explicit VectorObservable(const std::vector<T>& source);
    CancelableRef subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,E>>& observer) const override;
private:
    std::vector<T> source;

    static Task<Ack,None> pushEvent(
        unsigned int i,
        const std::vector<T>& source,
        std::shared_ptr<Scheduler> sched,
        std::shared_ptr<Observer<T,E>> observer,
        Ack lastAck
    );
};

template <class T, class E>
VectorObservable<T,E>::VectorObservable(const std::vector<T>& source)
    : source(source)
{}

template <class T, class E>
CancelableRef VectorObservable<T,E>::subscribe(
    const std::shared_ptr<Scheduler>& sched,
    const std::shared_ptr<Observer<T,E>>& observer) const
{
    return pushEvent(0, source, sched, observer, Continue)
        .doOnCancel(Task<None,None>::defer([observer] { return observer->onCancel(); }))
        .run(sched);
}

template <class T, class E>
Task<Ack,None> VectorObservable<T,E>::pushEvent(
    unsigned int i,
    const std::vector<T>& source,
    std::shared_ptr<Scheduler> sched,
    std::shared_ptr<Observer<T,E>> observer,
    Ack lastAck
) {
    if(i >= source.size()) {
        return observer->onComplete()
            .template map<Ack>([](auto) {
                return Stop;
            });
    } else if(lastAck == Continue) {
        auto value = source[i];
        return observer->onNext(value)
            .template flatMap<Ack>(std::bind(pushEvent, i + 1, source, sched, observer, _1));
    } else {
        return Task<Ack,None>::pure(Stop);
    }
}

}

#endif
