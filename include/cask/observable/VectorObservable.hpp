//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_VECTOR_OBSERVABLE_
#define _CASK_VECTOR_OBSERVABLE_

#include "../Observable.hpp"
#include "../Observer.hpp"

namespace cask::observable {

template <class T, class E>
class VectorObservable final : public Observable<T,E> {
public:
    template <typename Arg, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Arg>,
            std::vector<T>
        >::value
    >>
    explicit VectorObservable(Arg&& source)
        : source(std::forward<Arg>(source))
    {}

    FiberRef<None,None> subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,E>>& observer) const override;
private:
    std::vector<T> source;

    static Task<Ack,None> pushEvent(
        unsigned int i,
        const std::vector<T>& source,
        const std::shared_ptr<Scheduler>& sched,
        const std::shared_ptr<Observer<T,E>>& observer,
        Ack lastAck
    );
};

template <class T, class E>
FiberRef<None,None> VectorObservable<T,E>::subscribe(
    const std::shared_ptr<Scheduler>& sched,
    const std::shared_ptr<Observer<T,E>>& observer) const
{
    return Task<None,None>::defer([sched, observer, source = source] {
            return pushEvent(0, source, sched, observer, Continue)
                .template map<None>([](auto) { return None(); });
        })
     
        .doOnCancel(Task<None,None>::defer([observer] {
            return observer->onCancel();
        }))
        .run(sched);
}

template <class T, class E>
Task<Ack,None> VectorObservable<T,E>::pushEvent(
    unsigned int i,
    const std::vector<T>& source,
    const std::shared_ptr<Scheduler>& sched,
    const std::shared_ptr<Observer<T,E>>& observer,
    Ack lastAck
) {
    return Task<Ack, None>::defer([i, source, sched, observer, lastAck] {
        if(i >= source.size()) {
            return observer->onComplete()
                .template map<Ack>([](auto) {
                    return Stop;
                });
        } else if(lastAck == Continue) {
            T next = source[i];
            return observer->onNext(std::forward<T>(next))
                .template flatMap<Ack>([i, source, sched, observer](auto ack) {
                    return pushEvent(i + 1, source, sched, observer, ack);
                });
        } else {
            return Task<Ack,None>::pure(Stop);
        }
    });
}

} // namespace cask::observable

#endif
