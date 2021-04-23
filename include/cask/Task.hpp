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
#include "None.hpp"
#include "Scheduler.hpp"
#include "trampoline/TrampolineOp.hpp"
#include "trampoline/TrampolineRunLoop.hpp"

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
 * To evaluate a Task (or its composition) use the `evaluate` method which returns
 * a `Deferred`. At this point the computation is running and when a result is
 * available it will be provided there. At this point the evaluation is ongoing
 * and can result in side effects throughout the system. As a result, it's recommended
 * to evalute a task as close to the "end of the world" (i.e. the edge of your
 * application) as possible.
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
     * to the supplied method. The method is upplied a `Scheduler`
     * instance and must return a `Deferred` which wraps the ongoing
     * evaluation.
     * 
     * @param predicate The method to defer evalution to.
     * @return A task wrapping the given deferal function.
     */
    constexpr static Task<T,E> deferAction(const std::function<DeferredRef<T,E>(const std::shared_ptr<Scheduler>&)>& predicate) noexcept;

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
     * of the run can be observed via the returned `Deferred`
     * instance.
     * 
     * @param sched The scheduler to use for running of the task.
     * @return A `Deferred` reference to the running computation.
     */
    DeferredRef<T,E> run(const std::shared_ptr<Scheduler>& scheduler) const;

    /**
     * Attempt synchronous execution of this task. Either a synchronous
     * result will be provided or a task which, when run with a scheduler,
     * will provide an asynchronous result.
     * 
     * @return A value, an error, or a Task representing the asynchronous
     *         continuation of this computation.
     */
    Either<Either<T,E>,Task<T,E>> runSync() const;

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
    template <class T2>
    constexpr Task<Either<T,T2>,E> raceWith(const Task<T2,E>& other) const noexcept;

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
    constexpr explicit Task(const std::shared_ptr<trampoline::TrampolineOp>& op) noexcept;
    constexpr explicit Task(std::shared_ptr<trampoline::TrampolineOp>&& op) noexcept;
    constexpr Task(const Task<T,E>& other) noexcept;
    constexpr Task(Task<T,E>&& other) noexcept;
    constexpr Task<T,E>& operator=(const Task<T,E>& other) noexcept;
    constexpr Task<T,E>& operator=(Task<T,E>&& other) noexcept;

    std::shared_ptr<trampoline::TrampolineOp> op;
};

template <class T, class E>
constexpr Task<T,E> Task<T,E>::pure(const T& value) noexcept {
    return Task<T,E>(
        trampoline::TrampolineOp::value(value)
    );
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::raiseError(const E& error) noexcept {
    return Task<T,E>(
        trampoline::TrampolineOp::error(error)
    );
}

template <class T, class E>
constexpr Task<None,E> Task<T,E>::none() noexcept {
    return Task<None,E>::pure(None());
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::eval(const std::function<T()>& predicate) noexcept {
    return Task<T,E>(
        trampoline::TrampolineOp::thunk(predicate)
    );
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::defer(const std::function<Task<T,E>()>& predicate) noexcept {
    return Task<T,E>(
        trampoline::TrampolineOp::thunk(predicate)->flatMap(
            [](auto erased_task, auto isError) constexpr {
                if(isError) {
                    return trampoline::TrampolineOp::error(erased_task);
                } else {
                    auto task = erased_task.template get<Task<T,E>>();
                    return task.op;
                }
            }
        )
    );
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::deferAction(const std::function<DeferredRef<T,E>(const std::shared_ptr<Scheduler>&)>& predicate) noexcept {
    return Task<T,E>(
        trampoline::TrampolineOp::async([predicate](auto sched) {
            auto promise = Promise<Erased,Erased>::create(sched);
            auto deferred = predicate(sched);

            deferred->template chainDownstream<Erased,Erased>(promise, [](Either<T,E> result) {
                if(result.is_left()) {
                    return Either<Erased,Erased>::left(result.get_left());
                } else {
                    return Either<Erased,Erased>::right(result.get_right());
                }
            });

            return Deferred<Erased,Erased>::forPromise(promise);
        })
    );
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::forPromise(const PromiseRef<T,E>& promise) noexcept {
    return Task<T,E>(
        trampoline::TrampolineOp::async([promise](auto sched) {
            auto erasedPromise = Promise<Erased,Erased>::create(sched);
            auto deferred = Deferred<T,E>::forPromise(promise);

            deferred->template chainDownstream<Erased,Erased>(erasedPromise, [](Either<T,E> result) {
                if(result.is_left()) {
                    return Either<Erased,Erased>::left(result.get_left());
                } else {
                    return Either<Erased,Erased>::right(result.get_right());
                }
            });

            return Deferred<Erased,Erased>::forPromise(erasedPromise);
        })
    );
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::never() noexcept {
    return Task<T,E>(
        trampoline::TrampolineOp::async([](auto sched) constexpr {
            auto promise = Promise<Erased,Erased>::create(sched);
            return Deferred<Erased,Erased>::forPromise(promise);
        })
    );
}

template <class T, class E>
DeferredRef<T,E> Task<T,E>::run(const std::shared_ptr<Scheduler>& sched) const {
    auto result = trampoline::TrampolineRunLoop::execute(op,sched);

    if(auto either = std::get_if<Either<Erased,Erased>>(&result)) {
        if(either->is_left()) {
            auto success = either->get_left().template get<T>();
            return Deferred<T,E>::pure(success);
        } else {
            auto error = either->get_right().template get<E>();
            return Deferred<T,E>::raiseError(error);
        }
    } else {
        auto deferred = std::get<DeferredRef<Erased,Erased>>(result);
        auto promise = Promise<T,E>::create(sched);
        deferred->template chainDownstream<T,E>(promise, [](auto result) mutable {
            if(result.is_left()) {
                return Either<T,E>::left(result.get_left().template get<T>());
            } else {
                return Either<T,E>::right(result.get_right().template get<E>());
            }
        });
        return Deferred<T,E>::forPromise(promise);
    }
}

template <class T, class E>
Either<Either<T,E>,Task<T,E>> Task<T,E>::runSync() const {
    auto result = trampoline::TrampolineRunLoop::executeSync(op);

    if(auto either = std::get_if<Either<Erased,Erased>>(&result)) {
        if(either->is_left()) {
            auto success = either->get_left().template get<T>();
            auto syncResult = Either<T,E>::left(success);
            return Either<Either<T,E>,Task<T,E>>::left(syncResult);
        } else {
            auto error = either->get_right().template get<E>();
            auto syncResult = Either<T,E>::right(error);
            return Either<Either<T,E>,Task<T,E>>::left(syncResult);
        }
    } else {
        auto boundary = std::get<trampoline::AsyncBoundary>(result);
        auto asyncTask = Task<T,E>::deferAction([boundary](auto sched) {
            auto deferred = trampoline::TrampolineRunLoop::executeAsyncBoundary(boundary, sched);
            auto promise = Promise<T,E>::create(sched);
            deferred->template chainDownstream<T,E>(promise, [](auto result) mutable {
                if(result.is_left()) {
                    return Either<T,E>::left(result.get_left().template get<T>());
                } else {
                    return Either<T,E>::right(result.get_right().template get<E>());
                }
            });
            return Deferred<T,E>::forPromise(promise);
        });
        return Either<Either<T,E>,Task<T,E>>::right(asyncTask);
    }
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::asyncBoundary() const noexcept {
    return Task<None,E>::deferAction([](auto sched) {
        auto promise = Promise<None,E>::create(sched);
        promise->success(None());
        return Deferred<None,E>::forPromise(promise);
    }).template flatMap<T>([self = *this](auto) {
        return self;
    });
}

template <class T, class E>
template <class T2>
constexpr Task<T2,E> Task<T,E>::map(const std::function<T2(const T&)>& predicate) const noexcept {
    return Task<T2,E>(
        op->flatMap([predicate](auto erased_input, auto isError) {
            try {
                if(isError) {
                    return trampoline::TrampolineOp::error(erased_input);
                } else {
                    auto input = erased_input.template get<T>();
                    return trampoline::TrampolineOp::value(predicate(input));
                }
            } catch(E& error) {
                return trampoline::TrampolineOp::error(error);
            }
        })
    );
}

template <class T, class E>
template <class E2>
constexpr Task<T,E2> Task<T,E>::mapError(const std::function<E2(const E&)>& predicate) const noexcept {
    return Task<T,E2>(
        op->flatMap([predicate](auto erased_input, auto isError) {
            try {
                if(isError) {
                    auto input = erased_input.template get<E>();
                    return trampoline::TrampolineOp::error(predicate(input));
                } else {
                    return trampoline::TrampolineOp::value(erased_input);
                }
            } catch(E& error) {
                return trampoline::TrampolineOp::error(predicate(error));
            }
        })
    );
}


template <class T, class E>
template <class T2>
constexpr Task<T2,E> Task<T,E>::flatMap(const std::function<Task<T2,E>(const T&)>& predicate) const noexcept {
    return Task<T2,E>(
        op->flatMap([predicate](auto erased_input, auto isError) {
            try {
                if(isError) {
                    return trampoline::TrampolineOp::error(erased_input);
                } else {
                    auto input = erased_input.template get<T>();
                    auto resultTask = predicate(input);
                    return resultTask.op;
                }
            } catch(E& error) {
                return trampoline::TrampolineOp::error(error);
            }
        })
    );
}

template <class T, class E>
template <class E2>
constexpr Task<T,E2> Task<T,E>::flatMapError(const std::function<Task<T,E2>(const E&)>& predicate) const noexcept {
    return Task<T,E2>(
        op->flatMap([predicate](auto erased_input, auto isError) {
            try {
                if(isError) {
                    auto input = erased_input.template get<E>();
                    auto resultTask = predicate(input);
                    return resultTask.op;
                } else {
                    return trampoline::TrampolineOp::value(erased_input);
                }
            } catch(E2& error) {
                return trampoline::TrampolineOp::error(error);
            } catch(E& error) {
                auto resultTask = predicate(error);
                return resultTask.op;
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
            op->flatMap([successPredicate, errorPredicate](auto erased_input, auto isError) {
                try {
                    if(isError) {
                        auto input = erased_input.template get<E>();
                        auto resultTask = errorPredicate(input);
                        return resultTask.op;
                    } else {
                        auto input = erased_input.template get<T>();
                        auto resultTask = successPredicate(input);
                        return resultTask.op;
                    }
                } catch(E& error) {
                    auto resultTask = errorPredicate(error);
                    return resultTask.op;
                }
            })
        );
    } else {
        return Task<T2,E2>(
            op->flatMap([successPredicate, errorPredicate](auto erased_input, auto isError) {
                try {
                    if(isError) {
                        auto input = erased_input.template get<E>();
                        auto resultTask = errorPredicate(input);
                        return resultTask.op;
                    } else {
                        auto input = erased_input.template get<T>();
                        auto resultTask = successPredicate(input);
                        return resultTask.op;
                    }
                } catch(E2& error) {
                    return trampoline::TrampolineOp::error(error);
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
        op->flatMap([](auto input, auto isError) constexpr {
            if(isError) {
                return trampoline::TrampolineOp::value(input);
            } else {
                return trampoline::TrampolineOp::error(input);
            }
        })
    );
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::onError(const std::function<void(const E&)>& handler) const noexcept {
    return Task<T,E>(
        op->flatMap([handler](auto input, auto isError) {
            try {
                if(isError) {
                    auto error = input.template get<E>();
                    handler(error);
                    return trampoline::TrampolineOp::error(input);
                } else {
                    return trampoline::TrampolineOp::value(input);
                }
            } catch(E& error) {
                return trampoline::TrampolineOp::error(error);
            }
        })
    );
}

template <class T, class E>
constexpr Task<Either<T,E>,E> Task<T,E>::materialize() const noexcept {
    return Task<Either<T,E>,E>(
        op->flatMap([](auto input, auto isError) constexpr {
            if(isError) {
                auto error = input.template get<E>();
                return trampoline::TrampolineOp::value(Either<T,E>::right(error));
            } else {
                auto value = input.template get<T>();
                return trampoline::TrampolineOp::value(Either<T,E>::left(value));
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
        op->flatMap([](auto input, auto isError) constexpr {
            if(isError) {
                return trampoline::TrampolineOp::error(input);
            } else {
                auto value = input.template get<Either<T2,E>>();
                if(value.is_left()) {
                    return trampoline::TrampolineOp::value(value.get_left());
                } else {
                    return trampoline::TrampolineOp::error(value.get_right());
                }
            }
        })
    );
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::delay(uint32_t milliseconds) const noexcept {
    return Task<T,E>(
        trampoline::TrampolineOp::async([milliseconds, self = *this](auto sched) constexpr {
            auto promise = Promise<Erased,Erased>::create(sched);

            sched->submitAfter(milliseconds, [sched, self, promise]() constexpr {
                auto deferred = self.run(sched);

                deferred->template chainDownstream<Erased,Erased>(promise, [](auto value) {
                    if(value.is_left()) {
                        return Either<Erased,Erased>::left(value.get_left());
                    } else {
                        return Either<Erased,Erased>::right(value.get_right());
                    }
                });
            });

            return Deferred<Erased,Erased>::forPromise(promise);
        })
    );
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::recover(const std::function<T(const E&)>& predicate) const noexcept {
    return Task<T,E>(
        op->flatMap([predicate](auto erased_input, auto isError) {
            try {
                if(isError) {
                    auto input = erased_input.template get<E>();
                    return trampoline::TrampolineOp::value(predicate(input));
                } else {
                    return trampoline::TrampolineOp::value(erased_input);
                }
            } catch(E& error) {
                return trampoline::TrampolineOp::value(predicate(error));
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
template <class T2>
constexpr Task<Either<T,T2>,E> Task<T,E>::raceWith(const Task<T2,E>& other) const noexcept {
    return Task<Either<T,T2>,E>(
        trampoline::TrampolineOp::async([left = *this, right = other](auto sched) constexpr {
            auto promise = Promise<Erased,Erased>::create(sched);

            auto leftDeferred = left.run(sched);
            auto rightDeferred = right.run(sched);

            auto leftDeferredWeak = std::weak_ptr<Deferred<T,E>>(leftDeferred);
            auto rightDeferredWeak = std::weak_ptr<Deferred<T2,E>>(rightDeferred);

            leftDeferred->template chainDownstream<Erased,Erased>(promise, [rightDeferredWeak](auto result) {
                if(auto rightDeferred = rightDeferredWeak.lock()) {
                    rightDeferred->cancel();
                }

                if(result.is_left()) {
                    return Either<Erased,Erased>::left(
                        Either<T,T2>::left(result.get_left())
                    );
                } else {
                    return Either<Erased,Erased>::right(result.get_right());
                }
            });

            rightDeferred->template chainDownstream<Erased,Erased>(promise, [leftDeferredWeak](auto result) {
                if(auto leftDeferred = leftDeferredWeak.lock()) {
                    leftDeferred->cancel();
                }

                if(result.is_left()) {
                    return Either<Erased,Erased>::left(
                        Either<T,T2>::right(result.get_left())
                    );
                } else {
                    return Either<Erased,Erased>::right(result.get_right());
                }
            });

            return Deferred<Erased,Erased>::forPromise(promise);
        })
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
    return materialize()
    .template sideEffect<T2>(task)
    .template dematerialize<T>();
}

template <class T, class E>
constexpr Task<T,E> Task<T,E>::timeout(uint32_t milliseconds, const E& error) const noexcept {
    auto timeoutTask = Task<T,E>::raiseError(error).delay(milliseconds);

    return raceWith(timeoutTask).template map<T>([](auto result) {
        return result.get_left();
    });
}

template <class T, class E>
constexpr Task<T,E>::Task(const std::shared_ptr<trampoline::TrampolineOp>& op) noexcept
    : op(op)
{}

template <class T, class E>
constexpr Task<T,E>::Task(std::shared_ptr<trampoline::TrampolineOp>&& op) noexcept
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

}

#endif