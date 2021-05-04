//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_CALLBACK_OBSERVER_H_
#define _CASK_CALLBACK_OBSERVER_H_

#include "../Observer.hpp"

namespace cask::observable {

template <class T, class E>
class CallbackObserver final : public Observer<T,E> {
public:
    CallbackObserver(
        const std::function<Task<Ack,None>(const T&)>& onNextHdl,
        const std::function<Task<None,None>(const E&)>& onErrorHdl,
        const std::function<Task<None,None>()>& onCompleteHdl
    );

    Task<Ack,None> onNext(const T& value) override;
    Task<None,None> onError(const E& value) override;
    Task<None,None> onComplete() override;

private:
    std::function<Task<Ack,None>(const T&)> onNextHdl;
    std::function<Task<None,None>(const E&)> onErrorHdl;
    std::function<Task<None,None>()> onCompleteHdl;
};

template <class T, class E>
CallbackObserver<T,E>::CallbackObserver(
    const std::function<Task<Ack,None>(const T&)>& onNextHdl,
    const std::function<Task<None,None>(const E&)>& onErrorHdl,
    const std::function<Task<None,None>()>& onCompleteHdl
)   : onNextHdl(onNextHdl)
    , onErrorHdl(onErrorHdl)
    , onCompleteHdl(onCompleteHdl)
{}

template <class T, class E>
Task<Ack,None> CallbackObserver<T,E>::onNext(const T& value) {
    return onNextHdl(value);
}

template <class T, class E>
Task<None,None> CallbackObserver<T,E>::onError(const E& value) {
    return onErrorHdl(value);
}

template <class T, class E>
Task<None,None>CallbackObserver<T,E>::onComplete() {
    return onCompleteHdl();
}


}

#endif