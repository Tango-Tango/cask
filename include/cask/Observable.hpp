//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_OBSERVABLE_H_
#define _CASK_OBSERVABLE_H_

#include <any>
#include "BufferRef.hpp"
#include "Cancelable.hpp"
#include "Observer.hpp"
#include "Task.hpp"

namespace cask {

template <typename T, typename E>
class Observable;

template <typename T, typename E = std::any>
using ObservableRef = std::shared_ptr<Observable<T,E>>;

template <typename T, typename E = std::any>
using ObservableConstRef = std::shared_ptr<const Observable<T,E>>;

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
template <typename T, typename E = std::any>
class Observable : public std::enable_shared_from_this<Observable<T,E>> {
public:
    /**
     * Create an observable which immediately cancels any downstream
     * observer.
     * @return A new observable that cancels any downstream observers.
     */
    static ObservableRef<T,E> cancel();

    /**
     * Create an Observable housing a single pure value. When subscribed this
     * one value will be emitted and the stream will complete.
     * @param value The value to emit when subscribed.
     * @return A new observable wrapping the given value.
     */
    template <typename... Args>
    static ObservableRef<T,E> pure(Args&&... args);

    /**
     * Create an Observable that, upon subscription, immediately returns an error.
     *
     * @param error The error to emit when subscribed.
     * @return A new observable wrapping the given error.
     */
    template <typename... Args>
    static ObservableRef<T,E> raiseError(Args&&... args);

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
    template <typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<T()>
        >::value
    >>
    static ObservableRef<T,E> eval(Predicate&& predicate);

    /**
     * Create an observable which, upon subscription, defers that
     * subscription to the observable created by the provided
     * method.
     *
     * @param predicate The method to use for creating an observable.
     * @return An observable wrapping the given deferral function.
     */
    template <typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<ObservableRef<T,E>()>
        >::value
    >>
    static ObservableRef<T,E> defer(Predicate&& predicate);

    /**
     * Create an observable which, upon subscription, evaluates the
     * given task and provides its result as the single value in
     * the stream.
     *
     * @param predicate The method to use for creating a task.
     * @return An observable wrapping the given deferral function.
     */
    template <typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<Task<T,E>()>
        >::value
    >>
    static ObservableRef<T,E> deferTask(Predicate&& predicate);

    /**
     * Create and observable which repeatedly evaluates the given task and
     * supplies the resulting value to downstream observers.
     * @param task The task to repeatedly execute.
     * @return A new observable which will infinitely execute the given task
     *         until canceled or the observer stops the execution.
     */
    template <typename Arg, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Arg>,
            Task<T,E>
        >::value
    >>
    static ObservableRef<T,E> repeatTask(Arg&& task);

    /**
     * Create an observable which evalutes the given task a single time and
     * emites its result.
     * @param task The task to execute.
     * @return A new observable which will execute the give task a single time.
     */
    template <typename Arg, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Arg>,
            Task<T,E>
        >::value
    >>
    static ObservableRef<T,E> fromTask(Arg&& task);

    /**
     * Create an observable who emits each element of the given vector
     * to downstream observers.
     * @param vector The vector to iterator through and whose values to
     *               emit downstream.
     * @return A new observable which will emit the values of the given
     *         vector on subscription
     */
    template <typename Arg, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Arg>,
            std::vector<T>
        >::value
    >>
    static ObservableRef<T,E> fromVector(Arg&& vector);

    /**
     * Create an observable who emits each element of the given vector
     * to downstream observers.
     * @param vector The vector to iterator through and whose values to
     *               emit downstream.
     * @return A new observable which will emit the values of the given
     *         vector on subscription
     */
    template <typename... Args>
    static ObservableRef<T,E> sequence(Args&&... args);

    /**
     * Merge all of the given observables together to create a new observable. All of the
     * given observables will be evaluated concurrently and their results provided downstream
     * If downstream backpressures it will backpressure all upstream observables - causing them to
     * wait until downstream is ready to provide another value. How the results of each
     * upstream observable are merged is undefined, but the individual ordering of events
     * from each upstream will be maintained.
     * 
     * @param observables The observables to merge together.
     * @return An observable which emits merged events from all of the given upstream
     *         observables.
     */
    template <typename... Args>
    static ObservableRef<T,E> mergeAll(Args&&... observables);

    /**
     * Create an observable who never emits any elements and which
     * never completes.
     * 
     * @return A new observable which will never emit a value, error,
     *         or completion.
     */
    static ObservableRef<T,E> never();

    /**
     * Subscribe to the observer - beginning computation of the stream. Ongoing
     * computation may be cancled by using the returned cancelation handle.
     * 
     * @param sched The scheduler to use for execution pipeline steps.
     * @param observer The observer to attach to the stream.
     * @return The handle which may be used to cancel computation on the stream.
     */
    [[nodiscard]]
    virtual FiberRef<None,None> subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,E>>& observer) const = 0;

    /**
     * Subscribe to the observer - beginning computation of the stream. Ongoing
     * computation may be cancled by using the returned cancelation handle. This
     * method is provided as a convenience for subscribers who may not want to
     * implement the entire `Observer` interface and would rather pass lambdas
     * for each of the handler methods. Please look at the contract documented
     * as part of `Observer` for details on how these methods are called.
     * 
     * @param sched The scheduler to use for execution pipeline steps.
     * @param onNext The onNext event handler.
     * @param onError The onError event handler. By default does nothing.
     * @param onComplete The onComplete event handler. By default does nothing.
     * @param onCancel Provide an error for upstream cancelations. By default does nothing.
     * @return The handle which may be used to cancel computation on the stream.
     */
    template <
        typename NextPredicate,
        typename ErrorPredicate,
        typename CompletePredicate,
        typename CancelPredicate,
        typename = std::enable_if_t<
            std::is_convertible<
                std::remove_reference_t<NextPredicate>,
                std::function<Task<Ack,None>(T&&)>
            >::value &&
            std::is_convertible<
                std::remove_reference_t<ErrorPredicate>,
                std::function<Task<None,None>(E&&)>
            >::value &&
            std::is_convertible<
                std::remove_reference_t<CompletePredicate>,
                std::function<Task<None,None>()>
            >::value &&
            std::is_convertible<
                std::remove_reference_t<CancelPredicate>,
                std::function<Task<None,None>()>
            >::value 
        >
    >
    [[nodiscard]]
    FiberRef<None,None> subscribeHandlers(
        const std::shared_ptr<Scheduler>& sched,
        NextPredicate&& onNext,
        ErrorPredicate&& onError = [](E&&) { return Task<None,None>::none(); },
        CompletePredicate&& onComplete = [] { return Task<None,None>::none(); },
        CancelPredicate&& onCancel = [] { return Task<None,None>::none(); }
    ) const;

    /**
     * Append another observable to the end of this one. If the original observable completes
     * with error or never completes the the appened observable will never be subscribed - so
     * ordering and backpressure are preserved.
     * 
     * @param other The observable to append to this one.
     * @return A new observable which emits events from this observable first and then, on completion,
     *         subscribes to and emits events from the next one.
     */
    template <typename Arg, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Arg>,
            ObservableRef<T,E>
        >::value
    >>
    ObservableRef<T,E> appendAll(Arg&& other) const;

    /**
     * Buffer input up to the given size and emit the buffer downstream. At
     * stream close a buffer will be emitted containing whatever has been
     * accumulated since the last time a buffer was emitted downstream.
     * 
     * @param size The number of items to accumulate in the buffer.
     * @return An observable which emits buffered items.
     */
    ObservableRef<BufferRef<T>,E> buffer(uint32_t size) const;

    /**
     * Concatenate another observable to the end of this one. An alias
     * for appendAll with the same semantics as that operator.
     * 
     * @param other The observable to concatenate to this one.
     * @return A new observable which emits the events from this observable
     *         first and then, on completion, subscribes to and emits events
     *         from the next one.
     */
    template <typename Arg, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Arg>,
            ObservableRef<T,E>
        >::value
    >>
    ObservableRef<T,E> concat(Arg&& other) const;

    /**
     * Given a sequence of emit values suppress duplicate consecutive events
     * emitted by the source.
     * 
     * @return A new observable which removes duplicate consecutive events.
     */
    ObservableRef<T,E> distinctUntilChanged() const;

    /**
     * Given a sequence of emit values suppress duplicate consecutive events
     * emitted by the source using the provided comparator to check
     * equality.
     * 
     * @param comparator A comparator to use for testing the equality of
     *                   two values.
     * @return A new observable which removes duplicate consecutive events.
     */
    template <typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<bool(const T&, const T&)>
        >::value
    >>
    ObservableRef<T,E> distinctUntilChangedBy(Predicate&& comparator) const;

    /**
     * Transform each element of the stream using the provided transforming predicate
     * function.
     * 
     * @param predicate The function to apply to each element of the stream.
     * @return A new observable which transforms each element of the stream.
     */
    template <typename T2, typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<T2(T&&)>
        >::value
    >>
    ObservableRef<T2,E> map(Predicate&& predicate) const;

    /**
     * Transform each error of the stream using the provided transforming predicate
     * function.
     *
     * @param predicate The function to apply to each error of the stream.
     * @return A new observable which transforms each error of the stream.
     */
    template <typename E2, typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<E2(E&&)>
        >::value
    >>
    ObservableRef<T,E2> mapError(Predicate&& predicate) const;

    /**
     * Transform both success and error values by return a Task which
     * implements the transform. 
     * 
     * 1. When upstream provides a value the successPredicate is called. If
     *    it returns another success value then downstreams onNext is called. If
     *    it returns an error then downstreams onError is called and upstream
     *    is stopped.
     * 2. When upstream provides an error the errorPredicate is called. If it
     *    returns a success value then downstreams onNext is called and then
     *    downstream is completed with OnComplete. If it returns an error
     *    value then downstream's onError is called.
     * 
     * These semantics allow a stream to, in a single step, be transformed
     * to any downstream stream type.
     *
     * @param successPredicate The function to apply on each normal value
     *        provided by upstream.
     * @param errorPredicate The function to apply on error values provided
     *        by upstream.
     * @return A new observable which transforms both normal and error values
     *         according to the given predicates.
     */
    template <
        typename T2,
        typename E2,
        typename SuccessPredicate,
        typename ErrorPredicate,
        typename = std::enable_if_t<
            std::is_convertible<
                std::remove_reference_t<SuccessPredicate>,
                std::function<Task<T2,E2>(T&&)>
            >::value &&
            std::is_convertible<
                std::remove_reference_t<ErrorPredicate>,
                std::function<Task<T2,E2>(E&&)>
            >::value
        >
    >
    std::shared_ptr<Observable<T2,E2>> mapBothTask(
        SuccessPredicate&& successPredicate,
        ErrorPredicate&& errorPredicate
    ) const;

    /**
     * Transform each element of the stream using the providing transforming predicat
     * function which may execute synchronously or asynchronously in the context of the
     * returned task.
     *
     * @param predicate The function to apply to each element of the stream.
     * @return A new observable which transforms each element of the stream.
     */
    template <typename T2, typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<Task<T2,E>(T&&)>
        >::value
    >>
    ObservableRef<T2,E> mapTask(Predicate&& predicate) const;

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
    template <typename T2, typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<ObservableRef<T2,E>(T&&)>
        >::value
    >>
    ObservableRef<T2,E> flatMap(Predicate&& predicate) const;

     /**
     * For each element in stream emit either a value or nothing downstream.
     *
     * @param predicate The function to apply to each element of the stream and
     *                  which emits a value or nothing downstream.
     * @return An observable which applies the given transform to each element of the stream.
     */
    template <typename T2, typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<std::optional<T2>(T&&)>
        >::value
    >>
    ObservableRef<T2,E> flatMapOptional(Predicate&& predicate) const;

    /**
     * Apply a function to each element of the observable and the result of the previous
     * invocation of the function (using as seed value for the first element) evaluating
     * from left-to-right as a possibly infinite series. Observables emitted by the function
     * will be concatenated to the output and the value of the last element of the inner
     * observable provided to the next invocation of the method in the series.
     *
     * @param predicate The function to apply to each element of the stream along with some
     *                  accumulator or state. The method emits a new stream of events
     *                  downstream.
     * @return An observable which applies the given transform to each element of the stream.
     */
    template <typename T2, typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<ObservableRef<T2,E>(T2&&, T&&)>
        >::value
    >>
    ObservableRef<T2,E> flatScan(T2&& seed, Predicate&& predicate) const;

    /**
     * Merge the given observable with this observable. Both this observable and the other
     * observable will be evaluated concurrently and their results provided downstream. If
     * downstream backpressures it will backpressure both observables - causing them to
     * wait until downstream is ready to provide another value. How the results of each
     * upstream observable are merged is undefined, but the individual ordering of events
     * from each upstream will be maintained.
     * 
     * @param other The other observable to merge with this one.
     * @return An observable which emits merged events from both this observable and
     *         the given observable.
     */
    ObservableRef<T,E> merge(const ObservableRef<T,E>& other) const;

    /**
     * Run the given predicate for each provided value. The predicate returns an observable
     * which will be concurrently executed with any other observables returned by the precicate.
     * Their results will be merged and provided downstream. If downstream backpressures it will
     * backpressure all observables - causing them to wait until downstream is ready to provide
     * another value. How the results of each upstream observable are merged is undefined, but
     * the individual ordering of events from each upstream will be maintained.
     * 
     * @param predicate The function to apply to each element of the stream and
     *                  which emits a new stream of events downstream.
     * @return An observable which applies the given transform to each element of the stream
     *         and merges the resulting observables together.
     */
    template <typename T2, typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<ObservableRef<T2,E>(T&&)>
        >::value
    >>
    ObservableRef<T2,E> mergeMap(Predicate&& predicate) const;

    /**
     * Creates a new observable where upstream and downstream are run concurrently and
     * communicate via a queue of the given maximum size. When the queue reaches the
     * maximum size the upstream will be backpressured.
     * 
     * @param queue_size The maximum size of the queue.
     * @return An observable with a queue between upstream and downstream
     */
    ObservableRef<T,E> queue(uint32_t queue_size) const;

    /**
     * Apply a pure function to each element of the observable and the result of the previous
     * invocation of the function (using as seed value for the first element) evaluating
     * from left-to-right as a possibly infinite series. Values emitted by the function
     * will be emitted downstream and provided back to the next invocation of the function
     * once an upstream value is ready.
     *
     * @param predicate The function to apply to each element of the stream along with some
     *                  accumulator or state.
     * @return An observable which applies the given transform to each element of the stream.
     */
    template <typename T2, typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<T2(T2&&, T&&)>
        >::value
    >>
    ObservableRef<T2,E> scan(T2&& seed, Predicate&& predicate) const;

    /**
     * Apply a side effecting and possibly asyncrhonouse function to each element of the
     * observable and the result of the previous invocation of the function (using as seed
     * value for the first element) evaluating from left-to-right as a possibly infinite
     * series. Values emitted by the function will be emitted downstream and provided back
     * to the next invocation of the function once an upstream value is ready.
     *
     * @param predicate The function to apply to each element of the stream along with some
     *                  accumulator or state.
     * @return An observable which applies the given transform to each element of the stream.
     */
    template <typename T2, typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<Task<T2,E>(T2&&, T&&)>
        >::value
    >>
    ObservableRef<T2,E> scanTask(T2&& seed, Predicate&& predicate) const;

    /**
     * For each element in stream emit a possible infinite series of elements
     * downstream. Upstream events will not be be backpressured. When an upstream
     * event occurs the downstream subscription will be canceled, the predicate will
     * be called to generate a new observable, and that observable will be subscribed.
     *
     * @param predicate The function to apply to each element of the stream and
     *                  which emits a new stream of events downstream.
     * @return An observable which applies the given transform to each element of the stream.
     */
    template <typename T2, typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<ObservableRef<T2,E>(T&&)>
        >::value
    >>
    ObservableRef<T2,E> switchMap(Predicate&& predicate) const;

    /**
     * Given a nested observable (an observable who emits other observables)
     * flatten them by evaluating each inner observable in-order and
     * emmitting those values downstream.
     * 
     * NOTE: This method implements the same behavior as `concatMap` from rx.
     * 
     * @return An observable which evaluates each inner observable in-order and
     *         emits its values downstream.
     */
    template <typename T2, typename = std::enable_if_t<
        std::is_assignable<
            ObservableRef<T2,E>,
            T
        >::value
    >>
    ObservableRef<T2,E> flatten() const;

    /**
     * Filter events keeping only those for which the given predicate function
     * returns true.
     * 
     * @param predicate A function which is evaluated for every event and
     *                  should return true for any event which should be
     *                  emitted downstream and false for any event which
     *                  should be dropped.
     * @return An observerable who emits only values which match the given
     *         predicate function.
     */
    template <typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<bool(const T&)>
        >::value
    >>
    ObservableRef<T,E> filter(Predicate&& predicate) const;

    /**
     * Run the given predicate for every element contained with the observable.
     * 
     * @param predicate The method to execute for every element.
     * @return A task which completes when the given predicate has been
     *         executed for every element of the observable.
     */
    template <typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<void(T&&)>
        >::value
    >>
    Task<None,E> foreach(Predicate&& predicate) const;

    /**
     * Run the given predicate for every element contained with the observable. The
     * predicate returns a task who may complete either synchronously or asynchronously.
     * 
     * @param predicate The method to execute for every element.
     * @return A task which completes when the given predicate has been
     *         executed for every element of the observable.
     */
    template <typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<Task<None,E>(T&&)>
        >::value
    >>
    Task<None,E> foreachTask(Predicate&& predicate) const;

    /**
     * Emit the last value of this stream seen. Note that if the stream of
     * values is infinite this task will never complete.
     * 
     * @return A task which, upon evaluation, will provide the last entry
     *         seen in the stream or nothing (if the stream was empty).
     */
    Task<std::optional<T>,E> last() const;

    /**
     * When this observable has processed all available values return
     * an empty task denoting that completion. Useful for cases where
     * a user would like to ensure processing of an observable completes
     * but otherwise does not care what the results were.
     * 
     * @return A task which, upon evaluation, will evaluate the complete
     *         observable and provide a None result on completion.
     */
    Task<None,E> completed() const;

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
    Task<std::vector<T>,E> take(uint32_t amount) const;

    /**
     * Consume from the upstream observable and emit downstream while the
     * given predicate function returns true. Once the predicate returns false
     * send the stop signal upstream and complete the downstream observers - thus
     * shutting down the stream. Values emitted downstream only include those for
     * which the predicate returned true - it is not inclusive of the final value
     * evaluated by the predicate.
     *
     * @param predicate The method to apply to each element of the stream - continuing
     *                  streaming while it returns true.
     * @return An observable that emits only the first elements matching the given
     *         predicate function.
     */
    template <typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<bool(const T&)>
        >::value
    >>
    ObservableRef<T,E> takeWhile(Predicate&& predicate) const;

    /**
     * Consume from the upstream observable and emit downstream while the
     * given predicate function returns true. Once the predicate returns false
     * send the stop signal upstream and complete the downstream observers - thus
     * shutting down the stream.V alues emitted downstream are inclusive of the final
     * value evaluated by the predicate (where it returned false).
     *
     * @param predicate The method to apply to each element of the stream - continuing
     *                  streaming while it returns true.
     * @return An observable that emits only the first elements matching the given
     *         predicate function.
     */
    template <typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<bool(const T&)>
        >::value
    >>
    ObservableRef<T,E> takeWhileInclusive(Predicate&& predicate) const;

    /**
     * Ensure the given task will be run when this observer completes on success,
     * error, or the subscription is cancelled.
     * 
     * @param task The task to ensure is executed in all shutdown cases.
     * @return An observable for which the given task is guaranteed to execute
     *         before shutdown.
     */
    template <typename Arg, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Arg>,
            Task<None,None>
        >::value
    >>
    ObservableRef<T,E> guarantee(Arg&& task) const;

    virtual ~Observable();
};

} // namespace cask

#include "observable/AppendAllObservable.hpp"
#include "observable/BufferObservable.hpp"
#include "observable/CancelObservable.hpp"
#include "observable/CallbackObserver.hpp"
#include "observable/DeferObservable.hpp"
#include "observable/DeferTaskObservable.hpp"
#include "observable/DistinctUntilChangedObservable.hpp"
#include "observable/EmptyObservable.hpp"
#include "observable/EvalObservable.hpp"
#include "observable/FilterObservable.hpp"
#include "observable/FlatMapObservable.hpp"
#include "observable/FlatScanObservable.hpp"
#include "observable/GuaranteeObservable.hpp"
#include "observable/LastObserver.hpp"
#include "observable/MapObservable.hpp"
#include "observable/MapErrorObservable.hpp"
#include "observable/MapTaskObservable.hpp"
#include "observable/MapBothTaskObservable.hpp"
#include "observable/MergeObservable.hpp"
#include "observable/QueueObservable.hpp"
#include "observable/RepeatTaskObservable.hpp"
#include "observable/ScanTaskObservable.hpp"
#include "observable/SwitchMapObservable.hpp"
#include "observable/TakeObserver.hpp"
#include "observable/TakeWhileObservable.hpp"
#include "observable/VectorObservable.hpp"

namespace cask {

template <typename T, typename E>
ObservableRef<T,E> Observable<T,E>::cancel() {
    return std::make_shared<observable::CancelObservable<T,E>>();
}

template <typename T, typename E>
template <typename... Args>
ObservableRef<T,E> Observable<T,E>::pure(Args&&... args) {
    auto task = Task<T,E>::pure(std::forward<Args>(args)...);
    return deferTask([task = std::move(task)] {
        return task;
    });
}

template <typename T, typename E>
template <typename... Args>
ObservableRef<T,E> Observable<T,E>::raiseError(Args&&... args) {
    auto task = Task<T,E>::raiseError(std::forward<Args>(args)...);
    return deferTask([task = std::move(task)] {
        return task;
    });
}

template <typename T, typename E>
ObservableRef<T,E> Observable<T,E>::empty() {
    return std::make_shared<observable::EmptyObservable<T,E>>();
}

template <typename T, typename E>
template <typename Predicate, typename>
ObservableRef<T,E> Observable<T,E>::eval(Predicate&& predicate) {
    return std::make_shared<observable::EvalObservable<T,E>>(std::forward<Predicate>(predicate));
}

template <typename T, typename E>
template <typename Predicate, typename>
ObservableRef<T,E> Observable<T,E>::defer(Predicate&& predicate) {
    return std::make_shared<observable::DeferObservable<T,E>>(std::forward<Predicate>(predicate));
}

template <typename T, typename E>
template <typename Predicate, typename>
ObservableRef<T,E> Observable<T,E>::deferTask(Predicate&& predicate) {
    return std::make_shared<observable::DeferTaskObservable<T,E>>(std::forward<Predicate>(predicate));
}

template <typename T, typename E>
template <typename Arg, typename>
std::shared_ptr<Observable<T,E>> Observable<T,E>::repeatTask(Arg&& task) {
    return std::make_shared<observable::RepeatTaskObservable<T,E>>(std::forward<Task<T,E>>(task));
}

template <typename T, typename E>
template <typename Arg, typename>
ObservableRef<T,E> Observable<T,E>::fromTask(Arg&& task) {
    return deferTask([task = std::forward<Task<T,E>>(task)](){ return task; });
}

template <typename T, typename E>
template <typename Arg, typename>
ObservableRef<T,E> Observable<T,E>::fromVector(Arg&& vector) {
    return std::make_shared<observable::VectorObservable<T,E>>(std::forward<Arg>(vector));
}

template <typename T, typename E>
template <typename... Args>
ObservableRef<T,E> Observable<T,E>::sequence(Args&&... args) {
    std::vector<T> v({std::forward<Args>(args)...});
    return std::make_shared<observable::VectorObservable<T,E>>(std::move(v));
}

template <typename T, typename E>
template <typename... Args>
ObservableRef<T,E> Observable<T,E>::mergeAll(Args&&... observables) {
    std::vector<ObservableConstRef<T,E>> v({std::forward<Args>(observables)...});
    auto all_observables = Observable<ObservableConstRef<T,E>,E>::fromVector(std::move(v));
    return std::make_shared<observable::MergeObservable<T,E>>(all_observables);
}

template <typename T, typename E>
ObservableRef<T,E> Observable<T,E>::never() {
    return Observable<T,E>::deferTask([] {
        return Task<T,E>::never();
    });
}

template <typename T, typename E>
template <
    typename NextPredicate,
    typename ErrorPredicate,
    typename CompletePredicate,
    typename CancelPredicate,
    typename
>
FiberRef<None,None> Observable<T,E>::subscribeHandlers(
    const std::shared_ptr<Scheduler>& sched,
    NextPredicate&& onNext,
    ErrorPredicate&& onError,
    CompletePredicate&& onComplete,
    CancelPredicate&& onCancel
) const {
    auto observer = std::make_shared<observable::CallbackObserver<T,E>>(
        std::forward<NextPredicate>(onNext),
        std::forward<ErrorPredicate>(onError),
        std::forward<CompletePredicate>(onComplete),
        std::forward<CancelPredicate>(onCancel)
    );

    return subscribe(sched, observer);
}

template <typename T, typename E>
template <typename Arg, typename>
ObservableRef<T,E> Observable<T,E>::appendAll(Arg&& arg) const {
    auto self = this->shared_from_this();
    return std::make_shared<observable::AppendAllObservable<T,E>>(std::move(self), std::forward<ObservableRef<T,E>>(arg));
}

template <typename T, typename E>
std::shared_ptr<Observable<BufferRef<T>,E>> Observable<T,E>::buffer(uint32_t size) const {
    auto self = this->shared_from_this();
    return std::make_shared<observable::BufferObservable<T,E>>(self, size);
}

template <typename T, typename E>
template <typename Arg, typename>
ObservableRef<T,E> Observable<T,E>::concat(Arg&& arg) const {
    return appendAll(std::forward<Arg>(arg));
}

template <typename T, typename E>
ObservableRef<T,E> Observable<T,E>::distinctUntilChanged() const {
    return distinctUntilChangedBy([](const auto& left, const auto& right) { return left == right; });
}

template <typename T, typename E>
template <typename Predicate, typename>
ObservableRef<T,E> Observable<T,E>::distinctUntilChangedBy(Predicate&& comparator) const {
    auto self = this->shared_from_this();
    return std::make_shared<observable::DistinctUntilChangedObservable<T,E>>(self, std::forward<Predicate>(comparator));
}

template <typename T, typename E>
template <typename T2, typename Predicate, typename>
std::shared_ptr<Observable<T2,E>> Observable<T,E>::map(Predicate&& predicate) const {
    auto self = this->shared_from_this();
    return std::make_shared<observable::MapObservable<T,T2,E>>(self, std::forward<Predicate>(predicate));
}

template <typename T, typename E>
template <typename E2, typename Predicate, typename>
std::shared_ptr<Observable<T,E2>> Observable<T,E>::mapError(Predicate&& predicate) const {
    auto self = this->shared_from_this();
    return std::make_shared<observable::MapErrorObservable<T,E,E2>>(self, std::forward<Predicate>(predicate));
}

template <typename T, typename E>
template <typename T2, typename E2, typename SuccessPredicate, typename ErrorPredicate, typename>
std::shared_ptr<Observable<T2,E2>> Observable<T,E>::mapBothTask(
    SuccessPredicate&& successPredicate,
    ErrorPredicate&& errorPredicate
) const {
    auto self = this->shared_from_this();
    return std::make_shared<observable::MapBothTaskObservable<T,T2,E,E2>>(self, std::forward<SuccessPredicate>(successPredicate), std::forward<ErrorPredicate>(errorPredicate));
}

template <typename T, typename E>
template <typename T2, typename Predicate, typename>
std::shared_ptr<Observable<T2,E>> Observable<T,E>::mapTask(Predicate&& predicate) const {
    auto self = this->shared_from_this();
    return std::make_shared<observable::MapTaskObservable<T,T2,E>>(self, std::forward<Predicate>(predicate));
}

template <typename T, typename E>
template <typename T2, typename Predicate, typename>
ObservableRef<T2,E> Observable<T,E>::flatMap(Predicate&& predicate) const {
    auto self = this->shared_from_this();
    return std::make_shared<observable::FlatMapObservable<T,T2,E>>(self, std::forward<Predicate>(predicate));
}

template <typename T, typename E>
template <typename T2, typename Predicate, typename>
ObservableRef<T2,E> Observable<T,E>::flatMapOptional(Predicate&& predicate) const {
    return this->template map<std::optional<T2>>([predicate = std::forward<Predicate>(predicate)](auto&& value) {
        return predicate(std::forward<T>(value));
    })
    ->filter([](auto&& result_opt) {
        return result_opt.has_value();
    })
    ->template map<T2>([](auto&& result_opt) {
        return *result_opt;
    });
}

template <typename T, typename E>
template <typename T2, typename Predicate, typename>
ObservableRef<T2,E> Observable<T,E>::flatScan(T2&& seed, Predicate&& predicate) const {
    auto self = this->shared_from_this();
    return std::make_shared<observable::FlatScanObservable<T,T2,E>>(self, std::forward<T2>(seed), std::forward<Predicate>(predicate));
}


template <typename T, typename E>
ObservableRef<T,E> Observable<T,E>::merge(const ObservableRef<T,E>& other) const {
    std::vector<std::shared_ptr<const Observable<T,E>>> all_observables_vector {
        this->shared_from_this(),
        other
    };
    auto all_observables = Observable<std::shared_ptr<const Observable<T,E>>,E>::fromVector(all_observables_vector);
    return std::make_shared<observable::MergeObservable<T,E>>(all_observables);
}

template <typename T, typename E>
template <typename T2, typename Predicate, typename>
ObservableRef<T2,E> Observable<T,E>::mergeMap(Predicate&& predicate) const {
    auto map_observable = this->template map<ObservableConstRef<T2,E>>(std::forward<Predicate>(predicate));
    return std::make_shared<observable::MergeObservable<T2,E>>(std::move(map_observable));
}

template <typename T, typename E>
ObservableRef<T,E> Observable<T,E>::queue(uint32_t queue_size) const {
    auto self = this->shared_from_this();
    return std::make_shared<observable::QueueObservable<T,E>>(self, queue_size);
}

template <typename T, typename E>
template <typename T2, typename Predicate, typename>
ObservableRef<T2,E> Observable<T,E>::scan(T2&& seed, Predicate&& predicate) const {
    return scanTask<T2>(std::forward<T2>(seed), [predicate = std::forward<Predicate>(predicate)](auto&& acc, auto&& value) {
        return Task<T2,E>::pure(predicate(std::forward<T2>(acc), std::forward<T>(value)));
    });
}

template <typename T, typename E>
template <typename T2, typename Predicate, typename>
ObservableRef<T2,E> Observable<T,E>::scanTask(T2&& seed, Predicate&& predicate) const {
    auto self = this->shared_from_this();
    return std::make_shared<observable::ScanTaskObservable<T,T2,E>>(self, std::forward<T2>(seed), std::forward<Predicate>(predicate));
}

template <typename T, typename E>
template <typename T2, typename Predicate, typename>
ObservableRef<T2,E> Observable<T,E>::switchMap(Predicate&& predicate) const {
    auto self = this->shared_from_this();
    return std::make_shared<observable::SwitchMapObservable<T,T2,E>>(self, std::forward<Predicate>(predicate));
}

template <typename T, typename E>
template <typename T2, typename>
ObservableRef<T2,E> Observable<T,E>::flatten() const {
    return this->template flatMap<T2>([](auto inner){ return inner; });
}

template <typename T, typename E>
template <typename Predicate, typename>
ObservableRef<T,E> Observable<T,E>::filter(Predicate&& predicate) const {
    auto self = this->shared_from_this();
    return std::make_shared<observable::FilterObservable<T,E>>(self, std::forward<Predicate>(predicate));
}

template <typename T, typename E>
template <typename Predicate, typename>
Task<None,E> Observable<T,E>::foreach(Predicate&& predicate) const {
    return map<None>([predicate = std::forward<Predicate>(predicate)](auto&& value) {
        predicate(std::forward<T>(value));
        return None();
    })->completed();
}

template <typename T, typename E>
template <typename Predicate, typename>
Task<None,E> Observable<T,E>::foreachTask(Predicate&& predicate) const {
    return mapTask<None>([predicate = std::forward<Predicate>(predicate)](auto&& value) {
        return predicate(std::forward<T>(value));
    })->completed();
}

template <typename T, typename E>
Task<std::optional<T>,E> Observable<T,E>::last() const {
    auto self = this->shared_from_this();
    return Task<std::optional<T>,E>::deferFiber([self = self](auto sched) {
        auto promise = Promise<std::optional<T>,E>::create(sched);

        return Task<None,None>::deferAction([promise, self](auto sched) {
            auto observer = std::make_shared<observable::LastObserver<T,E>>(promise);
            auto subscription = self->subscribe(sched, observer);
            return Deferred<None,None>::forFiber(subscription);
        })
        .template flatMapBoth<std::optional<T>,E>(
            [promise](auto) {
                return Task<std::optional<T>,E>::forPromise(promise);
            },
            [promise](auto) {
                return Task<std::optional<T>,E>::forPromise(promise);
            }
        )
        .run(sched);
    });
}

template <typename T, typename E>
Task<None,E> Observable<T,E>::completed() const {
    return last().template map<None>([](auto) { return None(); });
}

template <typename T, typename E>
Task<std::vector<T>,E> Observable<T,E>::take(uint32_t amount) const {
    if(amount == 0) {
        return Task<std::vector<T>,E>::pure(std::vector<T>());
    } else {
        auto self = this->shared_from_this();
        
        return Task<std::vector<T>,E>::deferFiber([amount, self = self](auto sched) {
            auto promise = Promise<std::vector<T>,E>::create(sched);

            return Task<None,None>::deferAction([promise, amount, self](auto sched) {
                auto observer = std::make_shared<observable::TakeObserver<T,E>>(amount, promise);
                auto subscription = self->subscribe(sched, observer);
                return Deferred<None,None>::forFiber(subscription);
            })
            .template flatMapBoth<std::vector<T>,E>(
                [promise](auto) {
                    return Task<std::vector<T>,E>::forPromise(promise);
                },
                [promise](auto) {
                    return Task<std::vector<T>,E>::forPromise(promise);
                }
            )
            .run(sched);
        });
    }
}

template <typename T, typename E>
template <typename Predicate, typename>
ObservableRef<T,E> Observable<T,E>::takeWhile(Predicate&& predicate) const {
    auto self = this->shared_from_this();
    return std::make_shared<observable::TakeWhileObservable<T,E>>(self, std::forward<Predicate>(predicate), false);
}

template <typename T, typename E>
template <typename Predicate, typename>
ObservableRef<T,E> Observable<T,E>::takeWhileInclusive(Predicate&& predicate) const {
    auto self = this->shared_from_this();
    return std::make_shared<observable::TakeWhileObservable<T,E>>(self, std::forward<Predicate>(predicate), true);
}

template <typename T, typename E>
template <typename Arg, typename>
ObservableRef<T,E> Observable<T,E>::guarantee(Arg&& task) const {
    auto self = this->shared_from_this();
    return std::make_shared<observable::GuaranteeObservable<T,E>>(self, std::forward<Task<None,None>>(task));
}

template <typename T, typename E>
Observable<T,E>::~Observable() {}

} // namespace cask

#endif
