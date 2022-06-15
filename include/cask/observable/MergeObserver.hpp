//          Copyright Tango Tango, Inc. 2020 - 2022.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_MERGE_OBSERVER_H_
#define _CASK_MERGE_OBSERVER_H_

#include "../Observer.hpp"

namespace cask::observable {

template <class T, class E>
class MergeObserver final : public Observer<ObservableConstRef<T,E>,E>, public Observer<T,E> {
public:
    MergeObserver(const std::shared_ptr<Observer<T,E>>& downstream);
    Task<Ack,None> onNext(const ObservableConstRef<T,E>& upstream) override;
    Task<Ack,None> onNext(const T& value) override;
    Task<None,None> onError(const E& error) override;
    Task<None,None> onComplete() override;
    Task<None,None> onCancel() override;
private:
    std::shared_ptr<Observer<T,E>> downstream;
};

template <class T, class E>
MergeObserver<T,E>::MergeObserver(const std::shared_ptr<Observer<T,E>>& downstream)
    : downstream(downstream)
{}

template <class T, class E>
Task<Ack,None> MergeObserver<T,E>::onNext(const ObservableConstRef<T,E>&) {
    return Task<Ack,None>::pure(Continue);
}

template <class T, class E>
Task<Ack,None> MergeObserver<T,E>::onNext(const T&) {
    return Task<Ack,None>::pure(Continue);
}

template <class T, class E>
Task<None,None> MergeObserver<T,E>::onError(const E&) {
    return Task<Ack,None>::none();
}

template <class T, class E>
Task<None,None> MergeObserver<T,E>::onComplete() {
    return downstream->onComplete();
}

template <class T, class E>
Task<None,None> MergeObserver<T,E>::onCancel() {
    return downstream->onCancel();
}

}

#endif
