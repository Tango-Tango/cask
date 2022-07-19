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
        std::function<Task<Ack,None>(T&&)>&& onNextHdl,
        std::function<Task<None,None>(E&&)>&& onErrorHdl,
        std::function<Task<None,None>()>&& onCompleteHdl,
        std::function<Task<None,None>()>&& onCancelHdl
    );

    Task<Ack,None> onNext(T&& value) override;
    Task<None,None> onError(E&& value) override;
    Task<None,None> onComplete() override;
    Task<None,None> onCancel() override;

private:
    std::function<Task<Ack,None>(T&&)> onNextHdl;
    std::function<Task<None,None>(E&&)> onErrorHdl;
    std::function<Task<None,None>()> onCompleteHdl;
    std::function<Task<None,None>()> onCancelHdl;
};

template <class T, class E>
CallbackObserver<T,E>::CallbackObserver(
    std::function<Task<Ack,None>(T&&)>&& onNextHdl,
    std::function<Task<None,None>(E&&)>&& onErrorHdl,
    std::function<Task<None,None>()>&& onCompleteHdl,
    std::function<Task<None,None>()>&& onCancelHdl
)   : onNextHdl(std::move(onNextHdl))
    , onErrorHdl(std::move(onErrorHdl))
    , onCompleteHdl(std::move(onCompleteHdl))
    , onCancelHdl(std::move(onCancelHdl))
{}

template <class T, class E>
Task<Ack,None> CallbackObserver<T,E>::onNext(T&& value) {
    return onNextHdl(std::forward<T>(value));
}

template <class T, class E>
Task<None,None> CallbackObserver<T,E>::onError(E&& value) {
    return onErrorHdl(std::forward<E>(value));
}

template <class T, class E>
Task<None,None>CallbackObserver<T,E>::onComplete() {
    return onCompleteHdl();
}

template <class T, class E>
Task<None,None>CallbackObserver<T,E>::onCancel() {
    return onCancelHdl();
}

} // namespace cask::observable

#endif