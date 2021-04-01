//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_OBSERVER_H_
#define _CASK_OBSERVER_H_

#include "Task.hpp"

namespace cask {

/**
 * An `Ack` is provided as feedback from observers to observables
 * after events are processed to signal that either (a) processing
 * should continue by forwarding the next event or (b) processing
 * should stop altogether and the stream should shutdown.
 */
enum Ack { Continue, Stop };

template <class T, class E>
class Observer;

template <class T, class E = std::any>
using ObserverRef = std::shared_ptr<Observer<T,E>>;

/**
 * An observer represents the consumer of an event stream.
 * 
 * Events are given to the consumer by calling `onNext`. The
 * processing of an event may proceed synchronously or asynchronously
 * as dictacted by the returned task. When processing of an event
 * is complete, observers signal back upstream to either `Continue`
 * sending the next event or `Stop` sending events altogether.
 * 
 * When the event stream completes, the observer will be notified
 * via a call to `onComplete`. This may not happen though - streams
 * can be cancelled without completing gracefully - and so observers
 * should be prepared to be destructed without a call to onComplete.
 * 
 * Observers _may_ be stateful - meaning each call to `onNext` manipulates
 * some state in such a way that the next event will be processed differently.
 * 
 * Observables must implement the following rules when calling methods
 * on this interface:
 * 
 *   1. `onNext` may be called 0 or more times but never concurrently.
 *   2. Callers must wait for the processing of an event to complete as
 *      signaled by the task returned by calls to `onNext`.
 *   3. Callers must stop calling `onNext` if an event results in a
 *      `Stop` event being returned to the caller.
 *   4. Callers may call `onComplete` 0 or 1 times. Once `onComplete` is
 *      called the caller must not call `onNext` again.
 *   5. Callers may call `onError` 0 or 1 times. After `onError` is called
 *      it must not be called again. Callers must also not call `onComplete`
 *      or `onNext` after this point.
 *   6. If an observer reports an error upstream via its task result, the
 *      upstream task must not call `onError` on the observer which reported
 *      that error.
 * 
 * These rules, when taken together, allow observers to both backpressure
 * their upstream sources and also allow observers to implement their `onNext`
 * methods without using expensive locks or other synchronization mechanisms.
 */
template <class T, class E>
class Observer : public std::enable_shared_from_this<Observer<T,E>> {
public:
    /**
     * Handle the next event in the event stream.
     * 
     * @param value The next value to process.
     * @return A signal to the upstream observable that processing
     *         can continue or needs to stop.
     */
    virtual Task<Ack,None> onNext(T value) = 0;

    virtual Task<None,None> onError(E error) = 0;

    /**
     * Handle stream close because all events have been processed.
     */
    virtual Task<None,None> onComplete() = 0;

    virtual ~Observer();
};

template <class T, class E>
Observer<T,E>::~Observer()
{}

}

#endif