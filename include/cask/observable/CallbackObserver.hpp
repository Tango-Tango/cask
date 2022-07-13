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
        const std::function<Task<Ack,None>(T&&)>& onNextHdl,
        const std::function<Task<None,None>(const E&)>& onErrorHdl,
        const std::function<Task<None,None>()>& onCompleteHdl,
        const std::function<Task<None,None>()>& onCancelHdl
    );

    Task<Ack,None> onNext(T&& value) override;
    Task<None,None> onError(const E& value) override;
    Task<None,None> onComplete() override;
    Task<None,None> onCancel() override;

private:
    std::function<Task<Ack,None>(T&&)> onNextHdl;
    std::function<Task<None,None>(const E&)> onErrorHdl;
    std::function<Task<None,None>()> onCompleteHdl;
    std::function<Task<None,None>()> onCancelHdl;
};

template <class T, class E>
CallbackObserver<T,E>::CallbackObserver(
    const std::function<Task<Ack,None>(T&&)>& onNextHdl,
    const std::function<Task<None,None>(const E&)>& onErrorHdl,
    const std::function<Task<None,None>()>& onCompleteHdl,
        const std::function<Task<None,None>()>& onCancelHdl
)   : onNextHdl(onNextHdl)
    , onErrorHdl(onErrorHdl)
    , onCompleteHdl(onCompleteHdl)
    , onCancelHdl(onCancelHdl)
{}

template <class T, class E>
Task<Ack,None> CallbackObserver<T,E>::onNext(T&& value) {
    return onNextHdl(std::forward<T>(value));
}

template <class T, class E>
Task<None,None> CallbackObserver<T,E>::onError(const E& value) {
    return onErrorHdl(value);
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