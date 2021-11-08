//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_CANCELABLE_H_
#define _CASK_CANCELABLE_H_

#include <any>
#include <memory>
#include <functional>

namespace cask {

class Cancelable;

using CancelableRef = std::shared_ptr<Cancelable>;

/**
 * Represents a computation that can be canceled.
 */
class Cancelable {
public:
    /**
     * Cancel an ongoing and uncompleted background computation.
     * Cancel may be called multiple times without error - the
     * cancellation will only be attempted once. When this method
     * returns the computation may not yet be cancelled - users
     * must observe the computation itself for an indication of
     * if and when it as cancelled.
     */
    virtual void cancel() = 0;

    /**
     * Register a callback to be processed in the event of a cancelation.
     *
     * @param callback The callback to run if this computation is canceled.
     * @return A handle which may be used to unregister the callback in the future.
     */
    virtual int onCancel(const std::function<void()>&) = 0;

    /**
     * Unregister the cancel callback identified by the given handle.
     * 
     * @param handle The handle for the callback you with to unregister.
     */
    virtual void unregisterCancelCallback(int handle) = 0;

    /**
     * Register a callback to be processed in the event the computation
     * completes without being cancelled.
     *
     * @param callback The callback to run if the computation is completed
     *                 without being cancelled.
     */
    virtual void onShutdown(const std::function<void()>& callback) = 0;

    virtual ~Cancelable() {};
};

class IgnoreCancelation final : public Cancelable {
public:
    void cancel() override {}
    int onCancel(const std::function<void()>&) override { return 0; }
    void onShutdown(const std::function<void()>&) override {}
    void unregisterCancelCallback(int) override {};

};

}

#endif