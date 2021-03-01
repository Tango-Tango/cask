//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_OBSERVABLE_H_
#define _CASK_OBSERVABLE_H_

#include <any>
#include "Cancelable.hpp"
#include "Observer.hpp"
#include "Task.hpp"

namespace cask {

template <class T, class E>
class Observable;

template <class T, class E = std::any>
using ObservableRef = std::shared_ptr<Observable<T,E>>;

/**
 * An Observable represents a stream of zero or more values and any possibl
 * asychronous computations associated with that stream. It is the streaming
 * evaluation peer to `Task`.
 * 
 * Observables are lazily evaluated pipelines of processing applied from 
 * a source (the "Observable") and signaled downstream to consumers
 * (the "Observer"). Evaluation of the stream begins at the time an
 * observer is attached via the `subscribe` method. If multiple observers
 * are attached to the same observable, each will construct their own complete
 * processing pipeline - nothing is shared.
 * 
 * These observables implement backpressure - meaning downstream observers signal
 * upstream when they would like work to be forwarded to them. Noisy producers are
 * responsible for buffering, dropping, or otherwise dealing with messages that
 * arrive while downstream consumers are busy processing.
 */
template <class T, class E = std::any>
class Observable : public std::enable_shared_from_this<Observable<T,E>> {
public:
    /**
     * Create an Observable housing a single pure value. When subscribed this
     * one value will be emitted and the stream will complete.
     * @param value The value to emit when subscribed.
     * @return A new observable wrapping the given value.
     */
    static ObservableRef<T,E> pure(const T& value);

    /**
     * Create an Observable that, upon subscription, immediately returns an error.
     *
     * @param error The error to emit when subscribed.
     * @return A new observable wrapping the given error.
     */
    static ObservableRef<T,E> raiseError(const E& error);

    /**
     * Create an empty observable that, upon subscription, immediately
     * completes the subscription.
     *
     * @return A new empty observable.
     */
    static ObservableRef<T,E> empty();

    /**
     * Create an observable which, upon subscription, evaluates the
     * given function and emmits its result. This event will be
     * emitted once and then the stream will complete.
     *
     * @param predicate The function to evaluate.
     * @return An observable wrapping the given function.
     */
    static ObservableRef<T,E> eval(std::function<T()> predicate);

    /**
     * Create an observable which, upon subscription, defers that
     * subscription to the observable created by the provided
     * method.
     *
     * @param predicate The method to use for creating an observable.
     * @return An observable wrapping the given deferral function.
     */
    static ObservableRef<T,E> defer(std::function<ObservableRef<T,E>()> predicate);

    /**
     * Create an observable which, upon subscription, evaluates the
     * given task and provides its result as the single value in
     * the stream.
     *
     * @param predicate The method to use for creating a task.
     * @return An observable wrapping the given deferral function.
     */
    static ObservableRef<T,E> deferTask(std::function<Task<T,E>()> predicate);

    /**
     * Create and observable which repeatedly evaluates the given task and
     * supplies the resulting value to downstream observers.
     * @param task The task to repeatedly execute.
     * @return A new observable which will infinitely execute the given task
     *         until canceled or the observer stops the execution.
     */
    static ObservableRef<T,E> repeatTask(Task<T,E> task);

    /**
     * Subscribe to the observer - beginning computation of the stream. Ongoing
     * computation may be cancled by using the returned cancelation handle.
     * 
     * @param sched The scheduler to use for execution pipeline steps.
     * @param observer The observer to attach to the stream.
     * @return The handle which may be used to cancel computation on the stream.
     */
    virtual CancelableRef<E> subscribe(std::shared_ptr<Scheduler> sched, std::shared_ptr<Observer<T,E>> observer) const = 0;

    /**
     * Transform each element of the stream using the provided transforming predicate
     * function.
     * 
     * @param predicate The function to apply to each element of the stream.
     * @return A new observable which transforms each element of the stream.
     */
    template <class T2>
    ObservableRef<T2,E> map(std::function<T2(T)> predicate) const;

    /**
     * Transform each element of the stream using the providing transforming predicat
     * function which may execute synchronously or asynchronously in the context of the
     * returned task.
     *
     * @param predicate The function to apply to each element of the stream.
     * @return A new observable which transforms each element of the stream.
     */
    template <class T2>
    ObservableRef<T2,E> mapTask(std::function<Task<T2,E>(T)> predicate) const;

    /**
     * For each element in stream emit a possible infinite series of elements
     * downstream. Upstream events will be backpressured until all events
     * from the newly created stream have been processed.
     *
     * NOTE: This method implements the same behavior as `concatMap` from rx.
     *
     * @param predicate The function to apply to each element of the stream and
     *                  which emits a new stream of events downstream.
     * @return An observable which applies the given transform to each element of the stream.
     */
    template <class T2>
    ObservableRef<T2,E> flatMap(std::function<ObservableRef<T2,E>(T)> predicate) const;

    /**
     * Emit the last value of this stream seen. Note that if the stream of
     * values is infinite this task will never complete.
     * 
     * @return A task which, upon evaluation, will provide the last entry
     *         seen in the stream or nothing (if the stream was empty).
     */
    Task<std::optional<T>,E> last() const;

    /**
     * Accumulate the given number of elements from the stream and return
     * them to the caller. If the stream finishes before the given number
     * elements could be accumulated then whatever elements were found
     * will be returned. Once accumulation finishes the stream will be
     * stopped.
     * 
     * @param amount The number of elements to accumulate frm the stream.
     * @return A vector containing the accumulated results.
     */
    Task<std::vector<T>,E> take(unsigned int amount) const;

    /**
     * Consume from the upstream observable and emit downstream while the
     * given predicate function returns true. Once the predicate returns false
     * send the stop signal upstream and complete the downstream observers - thus
     * shutting down the stream.
     *
     * @param predicate The method to apply to each element of the stream - continuing
     *                  streaming while it returns true.
     * @return An observable that emits only the first elements matching the given
     *         predicate function.
     */
    ObservableRef<T,E> takeWhile(std::function<bool(T)> predicate) const;

    virtual ~Observable();
};

}

#include "observable/DeferObservable.hpp"
#include "observable/DeferTaskObservable.hpp"
#include "observable/EmptyObservable.hpp"
#include "observable/EvalObservable.hpp"
#include "observable/FlatMapObservable.hpp"
#include "observable/LastObserver.hpp"
#include "observable/MapObservable.hpp"
#include "observable/MapTaskObservable.hpp"
#include "observable/RepeatTaskObservable.hpp"
#include "observable/TakeObserver.hpp"
#include "observable/TakeWhileObservable.hpp"

namespace cask {

template <class T, class E>
ObservableRef<T,E> Observable<T,E>::pure(const T& value) {
    return deferTask([value]() {
        return Task<T,E>::pure(value);
    });
}

template <class T, class E>
ObservableRef<T,E> Observable<T,E>::raiseError(const E& error) {
    return deferTask([error]() {
        return Task<T,E>::raiseError(error);
    });
}

template <class T, class E>
ObservableRef<T,E> Observable<T,E>::empty() {
    return std::make_shared<observable::EmptyObservable<T,E>>();
}

template <class T, class E>
ObservableRef<T,E> Observable<T,E>::eval(std::function<T()> predicate) {
    return std::make_shared<observable::EvalObservable<T,E>>(predicate);
}

template <class T, class E>
ObservableRef<T,E> Observable<T,E>::defer(std::function<ObservableRef<T,E>()> predicate) {
    return std::make_shared<observable::DeferObservable<T,E>>(predicate);
}

template <class T, class E>
ObservableRef<T,E> Observable<T,E>::deferTask(std::function<Task<T,E>()> predicate) {
    return std::make_shared<observable::DeferTaskObservable<T,E>>(predicate);
}

template <class T, class E>
std::shared_ptr<Observable<T,E>> Observable<T,E>::repeatTask(Task<T,E> task) {
    return std::make_shared<observable::RepeatTaskObservable<T,E>>(task);
}

template <class T, class E>
template <class T2>
std::shared_ptr<Observable<T2,E>> Observable<T,E>::map(std::function<T2(T)> predicate) const {
    auto self = this->shared_from_this();
    return std::make_shared<observable::MapObservable<T,T2,E>>(self, predicate);
}

template <class T, class E>
template <class T2>
std::shared_ptr<Observable<T2,E>> Observable<T,E>::mapTask(std::function<Task<T2,E>(T)> predicate) const {
    auto self = this->shared_from_this();
    return std::make_shared<observable::MapTaskObservable<T,T2,E>>(self, predicate);
}

template <class T, class E>
template <class T2>
ObservableRef<T2,E> Observable<T,E>::flatMap(std::function<ObservableRef<T2,E>(T)> predicate) const {
    auto self = this->shared_from_this();
    return std::make_shared<observable::FlatMapObservable<T,T2,E>>(self, predicate);
}

template <class T, class E>
Task<std::optional<T>,E> Observable<T,E>::last() const {
    auto self = this->shared_from_this();
    return Task<std::optional<T>,E>::deferAction([self = self](auto sched) {
        auto promise = Promise<std::optional<T>,E>::create(sched);
        auto observer = std::make_shared<observable::LastObserver<T,E>>(promise);
        auto subscription = self->subscribe(sched, observer);

        promise->onCancel([subscription](auto cancelError) {
            subscription->cancel(cancelError);
        });

        return Deferred<std::optional<T>,E>::forPromise(promise);
    });
}

template <class T, class E>
Task<std::vector<T>,E> Observable<T,E>::take(unsigned int amount) const {
    auto self = this->shared_from_this();
    return Task<std::vector<T>,E>::deferAction([self = self, amount](auto sched) {
        auto promise = Promise<std::vector<T>,E>::create(sched);
        auto observer = std::shared_ptr<Observer<T,E>>(new observable::TakeObserver<T,E>(amount, promise));
        auto subscription = self->subscribe(sched, observer);

        promise->onCancel([subscription](auto cancelError) {
            subscription->cancel(cancelError);
        });

        return Deferred<std::vector<T>,E>::forPromise(promise);
    });
}

template <class T, class E>
ObservableRef<T,E> Observable<T,E>::takeWhile(std::function<bool(T)> predicate) const {
    auto self = this->shared_from_this();
    return std::make_shared<observable::TakeWhileObservable<T,E>>(self, predicate);
}

template <class T, class E>
Observable<T,E>::~Observable() {}

}

#endif