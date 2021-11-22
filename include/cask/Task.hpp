//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_TASK_H_
#define _CASK_TASK_H_

#include <any>
#include <functional>
#include <memory>
#include <type_traits>
#include "Deferred.hpp"
#include "Fiber.hpp"
#include "None.hpp"
#include "Scheduler.hpp"

namespace cask {

/**
 * A Task represents a possibly asynchronous computation that is lazily evaluated.
 * As a result a Task represents a computation that is _yet to happen_ rather than
 * one that is _already running in the background_. This is important and powerful.
 * Tasks can be trivially retried or restarted - their evaluation delayed with timers
 * - and their composition upholds functional principles such as referential transparency.
 * 
 * Tasks are also trampolined. This means they model infinitely recursive evaluation
 * (via `flatMap`) without performing said recursion on the stack or worrying about
 * tail call optimizations. This can make writing recursive algorithms _far_ easier.
 * 
 * To evaluate a Task (or its composition) use the `run` method which returns
 * a `Fiber`. At this point the computation is running and when a result is
 * available it will be provided there. As part of evaluation the resulting `Fiber`
 * may also be executing side effects. As a result, it's recommended to evalute a
 * task as close to the "end of the world" (i.e. the edge of your application) as
 * possible.
 * 
 * A Task can be evaluated as many times as you want. Each evaluation will execute
 * independently and will re-compute the entire composition of tasks.
 */
template <class T = None, class E = std::any>
class Task {
public:
    /**
     * Create a task that wraps a simple pure value. Whenever
     * the task is evaluated it will simply return this value.
     * 
     * @param value The pure value for this task.
     * @return A task wrapping this pure value.
     */
    constexpr static Task<T,E> pure(const T& value) noexcept;

    /**
     * Create a task that wraps a simple pure error. Whenever
     * the task is evaluated it will simply return this error.
     * 
     * @param value The pure error for this task.
     * @return A task wrapping this pure error.
     */
    constexpr static Task<T,E> raiseError(const E& error) noexcept;

    /**
     * Create a pure task containing no value. Useful fo representing
     * task results which are pure side effect.
     *
     * @return A task representing the none value.
     */
    constexpr static Task<None,E> none() noexcept;

    /**
     * Create a task that wraps a function. Whenever the
     * task is evaluated it will simply execute the given
     * function and provide its result to downstream tasks.
     * 
     * @param predicate The function to run when the task is evaluated.
     * @return A task wrapping the given function.
     */
    constexpr static Task<T,E> eval(const std::function<T()>& predicate) noexcept;

    /**
     * Create a task that, upon evaluation, defers said evalution
     * to the supplied method. Uoon evaluate the method is called
     * the returned task is evaluatd.
     * 
     * @param predicate The method to defer evalution to.
     * @return A task wrapping the given deferal function.
     */
    constexpr static Task<T,E> defer(const std::function<Task<T,E>()>& predicate) noexcept;

    /**
     * Create a task that, upon evaluation, defers said evalution
     * to the supplied method. The method is supplied a `Scheduler`
     * instance and must return a `Deferred` which wraps the ongoing
     * evaluation.
     * 
     * @param predicate The method to defer evalution to.
     * @return A task wrapping the given deferal function.
     */
    constexpr static Task<T,E> deferAction(const std::function<DeferredRef<T,E>(const std::shared_ptr<Scheduler>&)>& predicate) noexcept;

    /**
     * Create a task that, upon evaluation, defers said evalution
     * to the supplied method. The method is supplied a `Scheduler`
     * instance and must return a `Fiber` which represents the ongoing
     * evaluation.
     * 
     * @param predicate The method to defer evalution to.
     * @return A task wrapping the given deferal function.
     */
    constexpr static Task<T,E> deferFiber(const std::function<FiberRef<T,E>(const std::shared_ptr<Scheduler>&)>& predicate) noexcept;

    /**
     * Create a task which wraps the given already created promise. The given
     * task will provide a value when the given promise completes.
     * 
     * @param promise The promise to wrap in a task.
     * @return The task wrapping the given promise.
     */
    constexpr static Task<T,E> forPromise(const PromiseRef<T,E>& promise) noexcept;

    /**
     * Creates a task that will never finish evaluation.
     *  
     * @return A task that will never finish evaluation.
     */
    constexpr static Task<T,E> never() noexcept;

    /**
     * Trigger execution of this task on the given scheduler. Results
     * of the run can be observed via the returned `Fiber` instance.
     * 
     * @param sched The scheduler to use for running of the task.
     * @return A `Fiber` reference to the running computation.
     */
    FiberRef<T,E> run(const std::shared_ptr<Scheduler>& scheduler) const;

    /**
     * Attempt synchronous execution of this task. Either a synchronous
     * result will be provided or no result will be provided at all.
     * 
     * @return A value, an error, or no result.
     */
    std::optional<Either<T,E>> runSync() const;

    /**
     * Force an asynchronous boundary causing this task to defer
     * its continued execution to the scheduler.
     * @return A `Task` that, when executed, will immediately defer
     *         execution to the scheduler.
     */
    constexpr Task<T,E> asyncBoundary() const noexcept;

    /**
     * Transform the result of this task by applying the given function.
     * 
     * @param predicate The function to use for transforming the result.
     * @return A new `Task` representing the new output value.
     */
    template <class T2>
    constexpr Task<T2,E> map(const std::function<T2(const T&)>& predicate) const noexcept;

    /**
     * Transform the failed result of this task to a new error type.
     * 
     * @param predicate The function to use for transforming the error.
     * @return A new `Task` representing the new transformed error value.
     */
    template <class E2>
    constexpr Task<T,E2> mapError(const std::function<E2(const E&)>& predicate) const noexcept;

    /**
     * Transform the result of this task by appying the given function
     * which also returns a task. The returned inner task will also be
     * evaluated and its result applied to the output.
     * 
     * This method is not only useful for chaining asynchronous
     * operations, but also for implementing recursive algorithms
     * because it trampolines any recursion via the scheduler.
     * 
     * @param predicate The function to use for transforming the result.
     * @return A new `Task` representing the new output value;
     */
    template <class T2>
    constexpr Task<T2,E> flatMap(const std::function<Task<T2,E>(const T&)>& predicate) const noexcept;

    /**
     * Transform the error result of this task by appying the given function
     * which also returns a task. The returned inner task will also be
     * evaluated and its result applied to the output.
     * 
     * This method is simiilar to `flatMap` but it acts whenever an
     * error occurs rather than a success value.
     * 
     * @param predicate The function to use for transforming the result.
     * @return A new `Task` representing the new output value;
     */
    template <class E2>
    constexpr Task<T,E2> flatMapError(const std::function<Task<T,E2>(const E&)>& predicate) const noexcept;

    /**
     * Transform both the error and success types of this task using the given
     * predicate functions. The successPredicate is called to transform normal
     * values into either a new success value or a new error value. The errorPredicate
     * is called to transform error values into either a success value (essentially
     * recovering from the error) or a new error value.
     * 
     * @param successPredicate The function to use for transforming success values.
     * @param errorPredicate The function to use for transforming error values.
     * @return A new `Task` with transformed success and error values
     */
    template <class T2, class E2>
    constexpr Task<T2,E2> flatMapBoth(
        const std::function<Task<T2,E2>(const T&)>& successPredicate,
        const std::function<Task<T2,E2>(const E&)>& errorPredicate
    ) const noexcept;

    /**
     * Transpose the success and error types - causing errors to be treated
     * as successes and vice versa.  This allows operations such as `map`
     * and `flatMap` to be performed on errors.
     *
     * @return A new `Task` with the transposed success and error types.
     */
    constexpr Task<E,T> failed() const noexcept;

    /**
     * Run the given handler as a side effect whenever an error is encountered.
     * 
     * @return A new `Task` which runs the given handler on errors.
     */
    constexpr Task<T,E> onError(const std::function<void(const E&)>& handler) const noexcept;

    /**
     * Execute the given handler if the task is canceled after being started.
     * 
     * @return A new `Task` shich runs the given handler on cancels.
     */
    constexpr Task<T,E> doOnCancel(const Task<None,None>& handler) const noexcept;

    /**
     * Materialize both values and errors into the success type so that they
     * may be operated on simulataneously.
     *
     * @return A new `Task` which materializers both values and errors into the
     *         success type.
     */
    constexpr Task<Either<T,E>,E> materialize() const noexcept;

    /**
     * Dematerialize the success type which represents both values and errors. 
     * This operation is the inverse of `materialize` and, afterwards, provides
     * a task where normal operations such as `map` and `flatMap` no longer
     * operate on discovered errors.
     *
     * @return A new `Task` which dematerializes the success type containing
     *         both values and errors.
     */
    template <class T2, typename std::enable_if<
        std::is_assignable<Either<T2,E>,T>::value
    >::type* = nullptr>
    constexpr Task<T2,E> dematerialize() const noexcept;

    /**
     * Delay the exeuction of the given tasks by some number of milliseconds.
     * Note that the given delay will be _at least_ the given number of
     * milliseconds but may be more. Do not use these delays for the purposes
     * of generating an accurate time.
     * 
     * @param milliseconds The number of milliseconds to delay the task by.
     * @return A new `Task` represening the delayed execution.
     */
    constexpr Task<T,E> delay(uint32_t milliseconds) const noexcept;

    /**
     * Recover from an error by transforming it into some success value.
     * 
     * @param predicate The recovery method.
     * @return A new `Task` that will recover from errors.
     */
    constexpr Task<T,E> recover(const std::function<T(const E&)>& predicate) const noexcept;

    /**
     * Restarts this task until the given predicate function returns true.
     * 
     * @param predicate Function to evaluate when deciding if the given
     *                  task should be restarted.
     * @return A new `Task` which restarts as needed.
     */
    constexpr Task<T,E> restartUntil(const std::function<bool(const T&)>& predicate) const noexcept;

    /**
     * Runs this task and the given other task concurrently and provides
     * the value of whichever tasks finishes first.
     * 
     * @param other The task to race the current task with.
     * @result The value of the task which finished first.
     */
    constexpr Task<T,E> raceWith(const Task<T,E>& other) const noexcept;

    /**
     * Runs the given task as a side effect whose success results are
     * do not effect the output of the original task.
     *
     * @param task The side effecting task.
     * @result A task which mirrors the results of the original task
     *         but with the added evaluation of the given side effect.
     */
    template <class T2>
    constexpr Task<T,E> sideEffect(const Task<T2, E>& task) const noexcept;

    /**
     * Guarantee that the given task will be run when this task completes
     * regardless of success or error.
     *
     * @param task The task to run on success or error.
     * @result A task which mirrors the results of the original task
     *         but with the added guaranteed evalution of the given task.
     */
    template <class T2>
    constexpr Task<T,E> guarantee(const Task<T2, E>& task) const noexcept;

    /**
     * Timeout this task after the given interval if it does not complete
     * and provide the given error as the result. The task that timed out
     * will be cancelled.
     * 
     * @param milliseconds The number of of milliseconds to wait before
     *        timing out the task.
     * @param error The error to provide in the event that a timeout occurs.
     * @result A task which will either provide the original result value
     *         or the timeout error if a timeout occurs.
     */
    constexpr Task<T,E> timeout(uint32_t milliseconds, const E& error) const noexcept;

    /**
     * Construct a task which wraps the given trampoline operations. This
     * should not be called directly and, instead, users should use provided
     * operators to build these operations automatically.
     */
    constexpr explicit Task(const std::shared_ptr<const fiber::FiberOp>& op) noexcept;
    constexpr explicit Task(std::shared_ptr<const fiber::FiberOp>&& op) noexcept;
    constexpr Task(const Task<T,E>& other) noexcept;
    constexpr Task(Task<T,E>&& other) noexcept;
    constexpr Task<T,E>& operator=(const Task<T,E>& other) noexcept;
    constexpr Task<T,E>& operator=(Task<T,E>&& other) noexcept;

    std::shared_ptr<const fiber::FiberOp> op;
};

template <class T, class E>
constexpr Task<T,E>::Task(const std::shared_ptr<const fiber::FiberOp>& op) noexcept
    : op(op)
{}

template <class T, class E>
constexpr Task<T,E>::Task(std::shared_ptr<const fiber::FiberOp>&& op) noexcept
    : op(std::move(op))
{}

template <class T, class E>
constexpr Task<T,E>::Task(const Task<T,E>& other) noexcept
    : op(other.op)
{}

template <class T, class E>
constexpr Task<T,E>::Task(Task<T,E>&& other) noexcept
    : op(std::move(other.op))
{}

template <class T, class E>
constexpr Task<T,E>& Task<T,E>::operator=(const Task<T,E>& other) noexcept{
    this->op = other.op;
    return *this;
}

template <class T, class E>
constexpr Task<T,E>& Task<T,E>::operator=(Task<T,E>&& other) noexcept{
    this->op = std::move(other.op);
    return *this;
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::pure(const T& value) noexcept {
    return Task<T,E>(
        fiber::FiberOp::value(value)
    );
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::raiseError(const E& error) noexcept {
    return Task<T,E>(
        fiber::FiberOp::error(error)
    );
}

template <class T, class E>
constexpr Task<None,E> Task<T,E>::none() noexcept {
    return Task<None,E>::pure(None());
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::eval(const std::function<T()>& predicate) noexcept {
    return Task<T,E>(
        fiber::FiberOp::thunk(predicate)
    );
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::defer(const std::function<Task<T,E>()>& predicate) noexcept {
    return Task<T,E>(
        fiber::FiberOp::thunk(predicate)->flatMap(
            [](auto fiber_value) constexpr {
                if(fiber_value.isValue()) {
                    auto task = fiber_value.underlying().template get<Task<T,E>>();
                    return task.op;
                } else if(fiber_value.isError()) {
                    return fiber::FiberOp::error(fiber_value.underlying());
                } else {
                    return fiber::FiberOp::cancel();
                }
            }
        )
    );
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::deferAction(const std::function<DeferredRef<T,E>(const std::shared_ptr<Scheduler>&)>& predicate) noexcept {
    return Task<T,E>(
        fiber::FiberOp::async([predicate](auto sched) {
            return predicate(sched)->template mapBoth<Erased,Erased>(
                [](auto value) { return value; },
                [](auto error) { return error; }
            );
        })
    );
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::deferFiber(const std::function<FiberRef<T,E>(const std::shared_ptr<Scheduler>&)>& predicate) noexcept {
    return Task<T,E>(
        fiber::FiberOp::async([predicate](auto sched) {
            auto fiber = predicate(sched)->template mapBoth<Erased,Erased>(
                [](auto value) { return value; },
                [](auto error) { return error; }
            );

            return Deferred<Erased,Erased>::forFiber(fiber);
        })
    );
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::forPromise(const PromiseRef<T,E>& promise) noexcept {
    return Task<T,E>(
        fiber::FiberOp::async([promise](auto) {
            return Deferred<T,E>::forPromise(promise)->template mapBoth<Erased,Erased>(
                [](auto value) { return value; },
                [](auto error) { return error; }
            );
        })
    );
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::never() noexcept {
    return Task<T,E>(
        fiber::FiberOp::async([](auto sched) constexpr {
            auto promise = Promise<Erased,Erased>::create(sched);
            return Deferred<Erased,Erased>::forPromise(promise);
        })
    );
}

template <class T, class E>
FiberRef<T,E> Task<T,E>::run(const std::shared_ptr<Scheduler>& sched) const {
    return Fiber<T,E>::run(op, sched);
}

template <class T, class E>
std::optional<Either<T,E>> Task<T,E>::runSync() const {
    return Fiber<T,E>::runSync(op);
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::asyncBoundary() const noexcept {
    return Task<T,E>(
        fiber::FiberOp::async([op = op](auto sched) {
            auto promise = Promise<Erased,Erased>::create(sched);
            promise->success(Erased());
            return Deferred<Erased,Erased>::forPromise(promise);
        })->flatMap([op = op](auto) {
            return op;
        })
    );
}

template <class T, class E>
template <class T2>
constexpr Task<T2,E> Task<T,E>::map(const std::function<T2(const T&)>& predicate) const noexcept {
    return Task<T2,E>(
        op->flatMap([predicate](auto fiber_value) {
            try {
                if(fiber_value.isValue()) {
                    auto input = fiber_value.underlying().template get<T>();
                    return fiber::FiberOp::value(predicate(input));
                } else if(fiber_value.isError()) {
                    return fiber::FiberOp::error(fiber_value.underlying());
                } else {
                    return fiber::FiberOp::cancel();
                }
            } catch(E& error) {
                return fiber::FiberOp::error(error);
            }
        })
    );
}

template <class T, class E>
template <class E2>
constexpr Task<T,E2> Task<T,E>::mapError(const std::function<E2(const E&)>& predicate) const noexcept {
    return Task<T,E2>(
        op->flatMap([predicate](auto fiber_value) {
            try {
                if(fiber_value.isValue()) {
                    return fiber::FiberOp::value(fiber_value.underlying());
                } else if(fiber_value.isError()) {
                    auto input = fiber_value.underlying().template get<E>();
                    return fiber::FiberOp::error(predicate(input));
                } else {
                    return fiber::FiberOp::cancel();
                }
            } catch(E& error) {
                return fiber::FiberOp::error(predicate(error));
            }
        })
    );
}


template <class T, class E>
template <class T2>
constexpr Task<T2,E> Task<T,E>::flatMap(const std::function<Task<T2,E>(const T&)>& predicate) const noexcept {
    return Task<T2,E>(
        op->flatMap([predicate](auto fiber_input) {
            try {
                if(fiber_input.isValue()) {
                    auto input = fiber_input.underlying().template get<T>();
                    auto resultTask = predicate(input);
                    return resultTask.op;
                } else if(fiber_input.isError()) {
                    return fiber::FiberOp::error(fiber_input.underlying());
                } else {
                    return fiber::FiberOp::cancel();
                }
            } catch(E& error) {
                return fiber::FiberOp::error(error);
            }
        })
    );
}

template <class T, class E>
template <class E2>
constexpr Task<T,E2> Task<T,E>::flatMapError(const std::function<Task<T,E2>(const E&)>& predicate) const noexcept {
    return Task<T,E2>(
        op->flatMap([predicate](auto fiber_input) {
            if(fiber_input.isValue()) {
                return fiber::FiberOp::value(fiber_input.underlying());
            } else if(fiber_input.isError()) {
                auto input = fiber_input.underlying().template get<E>();
                auto resultTask = predicate(input);
                return resultTask.op;
            } else {
                return fiber::FiberOp::cancel();
            }
        })
    );
}

template <class T, class E>
template <class T2, class E2>
constexpr Task<T2,E2> Task<T,E>::flatMapBoth(
    const std::function<Task<T2,E2>(const T&)>& successPredicate,
    const std::function<Task<T2,E2>(const E&)>& errorPredicate
) const noexcept {
    if constexpr (std::is_same<E,E2>::value) {
        return Task<T2,E2>(
            op->flatMap([successPredicate, errorPredicate](auto fiber_input) {
                try {
                    if(fiber_input.isValue()) {
                        auto input = fiber_input.underlying().template get<T>();
                        auto resultTask = successPredicate(input);
                        return resultTask.op;
                    } else if(fiber_input.isError()) {
                        auto input = fiber_input.underlying().template get<E>();
                        auto resultTask = errorPredicate(input);
                        return resultTask.op;
                    } else {
                        return fiber::FiberOp::cancel();
                    }
                } catch(E& error) {
                    auto resultTask = errorPredicate(error);
                    return resultTask.op;
                }
            })
        );
    } else {
        return Task<T2,E2>(
            op->flatMap([successPredicate, errorPredicate](auto fiber_input) {
                try {
                    if(fiber_input.isValue()) {
                        auto input = fiber_input.underlying().template get<T>();
                        auto resultTask = successPredicate(input);
                        return resultTask.op;
                    } else if(fiber_input.isError()) {
                        auto input = fiber_input.underlying().template get<E>();
                        auto resultTask = errorPredicate(input);
                        return resultTask.op;
                    } else {
                        return fiber::FiberOp::cancel();
                    }
                } catch(E2& error) {
                    return fiber::FiberOp::error(error);
                } catch(E& error) {
                    auto resultTask = errorPredicate(error);
                    return resultTask.op;
                }
            })
        );
    }
}

template <class T, class E>
constexpr Task<E,T> Task<T,E>::failed() const noexcept {
    return Task<E,T>(
        op->flatMap([](auto input) constexpr {
            if(input.isError()) {
                return fiber::FiberOp::value(input.underlying());
            } else if(input.isValue()) {
                return fiber::FiberOp::error(input.underlying());
            } else {
                return fiber::FiberOp::cancel();
            }
        })
    );
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::onError(const std::function<void(const E&)>& handler) const noexcept {
    return Task<T,E>(
        op->flatMap([handler](auto fiber_input) {
            try {
                if(fiber_input.isValue()) {
                    return fiber::FiberOp::value(fiber_input.underlying());
                } else if(fiber_input.isError()) {
                    auto error = fiber_input.underlying().template get<E>();
                    handler(error);
                    return fiber::FiberOp::error(fiber_input.underlying());
                } else {
                    return fiber::FiberOp::cancel();
                }
            } catch(E& error) {
                return fiber::FiberOp::error(error);
            }
        })
    );
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::doOnCancel(const Task<None,None>& handler) const noexcept {
    return Task<T,E>(
        op->flatMap([handler_op = handler.op](auto fiber_input) {
            if(fiber_input.isValue()) {
                return fiber::FiberOp::value(fiber_input.underlying());
            } else if(fiber_input.isError()) {
                return fiber::FiberOp::error(fiber_input.underlying());
            } else {
                return handler_op->flatMap([](auto) {
                    return fiber::FiberOp::cancel();
                });
            }
        })
    );
}

template <class T, class E>
constexpr Task<Either<T,E>,E> Task<T,E>::materialize() const noexcept {
    return Task<Either<T,E>,E>(
        op->flatMap([](auto fiber_value) constexpr {
            if(fiber_value.isValue()) {
                auto value = fiber_value.underlying().template get<T>();
                return fiber::FiberOp::value(Either<T,E>::left(value));
            } else if(fiber_value.isError()) {
                auto error = fiber_value.underlying().template get<E>();
                return fiber::FiberOp::value(Either<T,E>::right(error));
            } else {
                return fiber::FiberOp::cancel();
            }
        })
    );
}

template <class T, class E>
template <class T2, typename std::enable_if<
    std::is_assignable<Either<T2,E>,T>::value
>::type*>
constexpr Task<T2,E> Task<T,E>::dematerialize() const noexcept {
    return Task<T2,E>(
        op->flatMap([](auto fiber_input) constexpr {
            if(fiber_input.isValue()) {
                auto value = fiber_input.underlying().template get<Either<T2,E>>();
                if(value.is_left()) {
                    return fiber::FiberOp::value(value.get_left());
                } else {
                    return fiber::FiberOp::error(value.get_right());
                }
            } else if(fiber_input.isError()) {
                return fiber::FiberOp::error(fiber_input.underlying());
            } else {
                return fiber::FiberOp::cancel();
            }
        })
    );
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::delay(uint32_t milliseconds) const noexcept {
    return Task<T,E>(
        fiber::FiberOp::delay(milliseconds)->flatMap([op = this->op](auto result) {
            if(result.isCanceled()) {
                return fiber::FiberOp::cancel();
            } else {
                return op;
            }
        })
    );
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::recover(const std::function<T(const E&)>& predicate) const noexcept {
    return Task<T,E>(
        op->flatMap([predicate](auto fiber_input) {
            if(fiber_input.isValue()) {
                return fiber::FiberOp::value(fiber_input.underlying());
            } else if(fiber_input.isError()) {
                auto input = fiber_input.underlying().template get<E>();
                return fiber::FiberOp::value(predicate(input));
            } else {
                return fiber::FiberOp::cancel();
            }
        })
    );
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::restartUntil(const std::function<bool(const T&)>& predicate) const noexcept {
    return flatMap<T>([self = *this, predicate](auto value) constexpr {
        if(predicate(value)) {
            return Task<T,E>::pure(value);
        } else {
            return self.restartUntil(predicate);
        }
    });
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::raceWith(const Task<T,E>& other) const noexcept {
    return Task<T,E>(
        fiber::FiberOp::race({op, other.op})
    );
}

template <class T, class E>
template <class T2>
constexpr Task<T,E> Task<T,E>::sideEffect(const Task<T2, E>& task) const noexcept {
    return flatMap<T>([task](T result) constexpr {
        return task.template map<T>([result](T2) constexpr {
            return result;
        });
    });
}

template <class T, class E>
template <class T2>
constexpr Task<T,E> Task<T,E>::guarantee(const Task<T2, E>& task) const noexcept {
    return Task<T,E>(
        op->flatMap(
            [guaranteed_op = task.op](auto fiber_value) constexpr {
                return guaranteed_op->flatMap([fiber_value](auto guaranteed_value) {
                    if(guaranteed_value.isError()) {
                        return fiber::FiberOp::error(guaranteed_value.underlying());
                    } else if(guaranteed_value.isCanceled()) {
                        return fiber::FiberOp::cancel();
                    } else if(fiber_value.isValue()) {
                        return fiber::FiberOp::value(fiber_value.underlying());
                    } else if(fiber_value.isError()) {
                        return fiber::FiberOp::error(fiber_value.underlying());
                    } else {
                        return fiber::FiberOp::cancel();
                    }
                });
            }
        )
    );
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::timeout(uint32_t milliseconds, const E& error) const noexcept {
    auto timeoutTask = Task<T,E>::raiseError(error).delay(milliseconds);
    return raceWith(timeoutTask);
}

}

#endif