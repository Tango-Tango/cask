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
template <typename T = None, typename E = std::any>
class Task {
public:
    /**
     * Create a task that wraps a simple pure value. Whenever
     * the task is evaluated it will simply return this value.
     * 
     * @param value The pure value for this task.
     * @return A task wrapping this pure value.
     */
    template <typename... Args>
    static Task<T,E> pure(Args&&... args) noexcept {
        auto value = T(std::forward<Args>(args)...);
        auto erased = Erased(std::move(value));
        auto op = fiber::FiberOp::value(std::move(erased));
        return Task<T,E>(std::move(op));
    }

    /**
     * Create a task that wraps a simple pure error. Whenever
     * the task is evaluated it will simply return this error.
     * 
     * @param value The pure error for this task.
     * @return A task wrapping this pure error.
     */
    template <typename... Args>
    static Task<T,E> raiseError(Args&&... args) noexcept {
        auto error = E(std::forward<Args>(args)...);
        auto erased = Erased(std::move(error));
        auto op = fiber::FiberOp::error(std::move(erased));
        return Task<T,E>(std::move(op));
    }

    /**
     * Create a pure task containing no value. Useful fo representing
     * task results which are pure side effect.
     *
     * @return A task representing the none value.
     */
    static Task<None,E> none() noexcept {
        return Task<None,E>::pure(None());
    }

    static Task<T,E> cancel() noexcept {
        return Task<T,E>(fiber::FiberOp::cancel());
    }

    /**
     * Create a task that wraps a function. Whenever the
     * task is evaluated it will simply execute the given
     * function and provide its result to downstream tasks.
     * 
     * @param predicate The function to run when the task is evaluated.
     * @return A task wrapping the given function.
     */
    template <typename Predicate = std::function<T()>>
    static Task<T,E> eval(Predicate&& thunk) noexcept {
        return Task<T,E>(
            fiber::FiberOp::thunk(std::forward<Predicate>(thunk))
        );
    }

    /**
     * Create a task that, upon evaluation, defers said evalution
     * to the supplied method. Uoon evaluate the method is called
     * the returned task is evaluatd.
     * 
     * @param predicate The method to defer evalution to.
     * @return A task wrapping the given deferal function.
     */
    template <typename Predicate = std::function<Task<T,E>()>>
    static Task<T,E> defer(Predicate&& predicate) noexcept {
        return Task<Task<T,E>,E>::eval(std::forward<Predicate>(predicate))
            .template flatMap<T>([](auto&& task) { return std::forward<Task<T,E>>(task); });
    }

    /**
     * Create a task that, upon evaluation, defers said evalution
     * to the supplied method. The method is supplied a `Scheduler`
     * instance and must return a `Deferred` which wraps the ongoing
     * evaluation.
     * 
     * @param predicate The method to defer evalution to.
     * @return A task wrapping the given deferal function.
     */
    template <typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<DeferredRef<T,E>(const std::shared_ptr<Scheduler>&)>
        >::value
    >>
    static Task<T,E> deferAction(Predicate&& predicate) noexcept  {
        return Task<T,E>(
            fiber::FiberOp::async([predicate = std::forward<Predicate>(predicate)](const auto& sched) {
                return predicate(sched)->template mapBoth<Erased,Erased>(
                    [](auto&& value) { return value; },
                    [](auto&& error) { return error; }
                );
            })
        );
    }

    /**
     * Create a task that, upon evaluation, defers said evalution
     * to the supplied method. The method is supplied a `Scheduler`
     * instance and must return a `Fiber` which represents the ongoing
     * evaluation.
     * 
     * @param predicate The method to defer evalution to.
     * @return A task wrapping the given deferal function.
     */
    template <typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<FiberRef<T,E>(const std::shared_ptr<Scheduler>&)>
        >::value
    >>
    static Task<T,E> deferFiber(Predicate&& predicate) noexcept  {
        return Task<T,E>(
            fiber::FiberOp::async([predicate = std::forward<Predicate>(predicate)](const auto& sched) {
                auto fiber = predicate(sched)->template mapBoth<Erased,Erased>(
                    [](auto&& value) { return value; },
                    [](auto&& error) { return error; }
                );

                return Deferred<Erased,Erased>::forFiber(fiber);
            })
        );
    }

    /**
     * Create a task which wraps the given already created promise. The given
     * task will provide a value when the given promise completes.
     * 
     * @param promise The promise to wrap in a task.
     * @return The task wrapping the given promise.
     */
    template <typename Arg, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Arg>,
            PromiseRef<T,E>
        >::value
    >>
    static Task<T,E> forPromise(Arg&& promise) noexcept  {
        return Task<T,E>(
            fiber::FiberOp::async([promise = std::forward<Arg>(promise)](const auto&) {
                return Deferred<T,E>::forPromise(promise)->template mapBoth<Erased,Erased>(
                    [](auto value) { return value; },
                    [](auto error) { return error; }
                );
            })
        );
    }

    /**
     * Creates a task that will never finish evaluation.
     *  
     * @return A task that will never finish evaluation.
     */
    static Task<T,E> never() noexcept  {
        return Task<T,E>(
            fiber::FiberOp::async([](const auto& sched) {
                auto promise = Promise<Erased,Erased>::create(sched);
                return Deferred<Erased,Erased>::forPromise(promise);
            })
        );
    }

    /**
     * Trigger execution of this task on the given scheduler. Results
     * of the run can be observed via the returned `Fiber` instance.
     * 
     * @param sched The scheduler to use for running of the task.
     * @return A `Fiber` reference to the running computation.
     */
    [[nodiscard]] FiberRef<T,E> run(const std::shared_ptr<Scheduler>& sched) const  {
        return Fiber<T,E>::run(op, sched);
    }

    /**
     * Attempt synchronous execution of this task. Either a synchronous
     * result will be provided or no result will be provided at all.
     * 
     * @return A value, an error, or no result.
     */
    std::optional<Either<T,E>> runSync() const  {
        return Fiber<T,E>::runSync(op);
    }

    /**
     * Force an asynchronous boundary causing this task to defer
     * its continued execution to the scheduler.
     * @return A `Task` that, when executed, will immediately defer
     *         execution to the scheduler.
     */
    Task<T,E> asyncBoundary() const noexcept  {
        return Task<T,E>(
            fiber::FiberOp::cede()->flatMap([op = op](auto&& fiber_value) {
                if(fiber_value.isCanceled()) {
                    return fiber::FiberOp::cancel();
                } else {
                    return op;
                }
            })
        );
    }

    /**
     * Transform the result of this task by applying the given function.
     * 
     * @param predicate The function to use for transforming the result.
     * @return A new `Task` representing the new output value.
     */
    template <typename T2, typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<T2(T&&)>
        >::value
    >>
    Task<T2,E> map(Predicate&& predicate) const noexcept {
        return Task<T2,E>(
            op->flatMap([predicate = std::forward<Predicate>(predicate)](auto&& fiber_value) {
                try {
                    if(fiber_value.isValue()) {
                        auto input = fiber_value.underlying().template get<T>();
                        auto result = predicate(std::forward<T>(input));
                        auto erased = Erased(std::forward<T2>(result));
                        return fiber::FiberOp::value(std::move(erased));
                    } else if(fiber_value.isError()) {
                        return fiber::FiberOp::error(fiber_value.underlying());
                    } else {
                        return fiber::FiberOp::cancel();
                    }
                } catch(E& error) {
                    return fiber::FiberOp::error(Erased(error));
                }
            })
        );
    }

    /**
     * Transform the failed result of this task to a new error type.
     * 
     * @param predicate The function to use for transforming the error.
     * @return A new `Task` representing the new transformed error value.
     */
    template <typename E2, typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<E2(E&&)>
        >::value
    >>
    Task<T,E2> mapError(Predicate&& predicate) const noexcept {
        return Task<T,E2>(
            op->flatMap([predicate = std::forward<Predicate>(predicate)](auto&& fiber_value) {
                try {
                    if(fiber_value.isValue()) {
                        return fiber::FiberOp::value(fiber_value.underlying());
                    } else if(fiber_value.isError()) {
                        auto input = fiber_value.underlying().template get<E>();
                        auto result = predicate(std::forward<E>(input));
                        auto erased = Erased(std::forward<E2>(result));
                        return fiber::FiberOp::error(std::move(erased));
                    } else {
                        return fiber::FiberOp::cancel();
                    }
                } catch(E& error) {
                    return fiber::FiberOp::error(Erased(predicate(std::forward<E>(error))));
                }
            })
        );
    }

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
    template <typename T2, typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<Task<T2,E>(T&&)>
        >::value
    >>
    Task<T2,E> flatMap(Predicate&& predicate) const noexcept  {
        return Task<T2,E>(
            op->flatMap([predicate = std::forward<Predicate>(predicate)](auto&& fiber_input) {
                try {
                    if(fiber_input.isValue()) {
                        auto input = fiber_input.underlying().template get<T>();
                        auto resultTask = predicate(std::forward<T>(input));
                        return resultTask.op;
                    } else if(fiber_input.isError()) {
                        return fiber::FiberOp::error(fiber_input.underlying());
                    } else {
                        return fiber::FiberOp::cancel();
                    }
                } catch(E& error) {
                    return fiber::FiberOp::error(Erased(error));
                }
            })
        );
    }

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
    template <typename E2, typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<Task<T,E2>(E&&)>
        >::value
    >>
    Task<T,E2> flatMapError(Predicate&& predicate) const noexcept  {
        return Task<T,E2>(
            op->flatMap([predicate = std::forward<Predicate>(predicate)](auto&& fiber_input) {
                if(fiber_input.isValue()) {
                    return fiber::FiberOp::value(fiber_input.underlying());
                } else if(fiber_input.isError()) {
                    auto input = fiber_input.underlying().template get<E>();
                    auto resultTask = predicate(std::forward<E>(input));
                    return resultTask.op;
                } else {
                    return fiber::FiberOp::cancel();
                }
            })
        );
    }

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
    template <typename T2, typename E2, typename SuccessPredicate, typename ErrorPredicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<SuccessPredicate>,
            std::function<Task<T2,E2>(T&&)>
        >::value &&
        std::is_convertible<
            std::remove_reference_t<ErrorPredicate>,
            std::function<Task<T2,E2>(E&&)>
        >::value
    >>
    Task<T2,E2> flatMapBoth(
        SuccessPredicate&& successPredicate,
        ErrorPredicate&& errorPredicate
    ) const noexcept  {
        if constexpr (std::is_same<E,E2>::value) {
            return Task<T2,E2>(
                op->flatMap([successPredicate, errorPredicate](auto&& fiber_input) {
                    try {
                        if(fiber_input.isValue()) {
                            auto input = fiber_input.underlying().template get<T>();
                            auto resultTask = successPredicate(std::forward<T>(input));
                            return resultTask.op;
                        } else if(fiber_input.isError()) {
                            auto input = fiber_input.underlying().template get<E>();
                            auto resultTask = errorPredicate(std::forward<E>(input));
                            return resultTask.op;
                        } else {
                            return fiber::FiberOp::cancel();
                        }
                    } catch(E& error) {
                        auto resultTask = errorPredicate(std::forward<E>(error));
                        return resultTask.op;
                    }
                })
            );
        } else {
            return Task<T2,E2>(
                op->flatMap([successPredicate, errorPredicate](auto&& fiber_input) {
                    try {
                        if(fiber_input.isValue()) {
                            auto input = fiber_input.underlying().template get<T>();
                            auto resultTask = successPredicate(std::forward<T>(input));
                            return resultTask.op;
                        } else if(fiber_input.isError()) {
                            auto input = fiber_input.underlying().template get<E>();
                            auto resultTask = errorPredicate(std::forward<E>(input));
                            return resultTask.op;
                        } else {
                            return fiber::FiberOp::cancel();
                        }
                    } catch(E2& error) {
                        return fiber::FiberOp::error(Erased(std::forward<E2>(error)));
                    } catch(E& error) {
                        auto resultTask = errorPredicate(std::forward<E>(error));
                        return resultTask.op;
                    }
                })
            );
        }
    }

    /**
     * Transpose the success and error types - causing errors to be treated
     * as successes and vice versa.  This allows operations such as `map`
     * and `flatMap` to be performed on errors.
     *
     * @return A new `Task` with the transposed success and error types.
     */
    Task<E,T> failed() const noexcept  {
        return Task<E,T>(
            op->flatMap([](auto&& input) {
                if(input.isError()) {
                    auto error = input.underlying().template get<E>();
                    auto erased = Erased(std::forward<E>(error));
                    return fiber::FiberOp::value(std::move(erased));
                } else if(input.isValue()) {
                    auto value = input.underlying().template get<T>();
                    auto erased = Erased(std::forward<T>(value));
                    return fiber::FiberOp::error(std::move(erased));
                } else {
                    return fiber::FiberOp::cancel();
                }
            })
        );
    }

    /**
     * Run the given handler as a side effect whenever an error is encountered.
     * 
     * @return A new `Task` which runs the given handler on errors.
     */
    template <typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<void(const E&)>
        >::value
    >>
    Task<T,E> onError(Predicate&& handler) const noexcept  {
        return Task<T,E>(
            op->flatMap([handler = std::forward<Predicate>(handler)](auto&& fiber_input) {
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

    /**
     * Execute the given handler if the task is canceled after being started.
     * 
     * @return A new `Task` shich runs the given handler on cancels.
     */
    template <typename Arg, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Arg>,
            Task<None,None>
        >::value
    >>
    Task<T,E> doOnCancel(Arg&& handler) const noexcept  {
        return Task<T,E>(
            op->flatMap([handler = std::forward<Task<None,None>>(handler)](auto&& fiber_input) {
                if(fiber_input.isValue()) {
                    return fiber::FiberOp::value(fiber_input.underlying());
                } else if(fiber_input.isError()) {
                    return fiber::FiberOp::error(fiber_input.underlying());
                } else {
                    return handler.op->flatMap([](auto) {
                        return fiber::FiberOp::cancel();
                    });
                }
            })
        );
    }

    /**
     * Convert a received cancel signal into the given error.
     * 
     * @return A new `Task` which converts cancels to the given error.
     */
    template <typename Arg, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Arg>,
            E
        >::value
    >>
    Task<T,E> onCancelRaiseError(Arg&& error) const noexcept  {
        return Task<T,E>(
            op->flatMap([error = std::forward<E>(error)](auto&& fiber_input) {
                if(fiber_input.isValue()) {
                    return fiber::FiberOp::value(fiber_input.underlying());
                } else if(fiber_input.isError()) {
                    return fiber::FiberOp::error(fiber_input.underlying());
                } else {
                    return fiber::FiberOp::error(error);
                }
            })
        );
    }

    /**
     * Materialize both values and errors into the success type so that they
     * may be operated on simulataneously.
     *
     * @return A new `Task` which materializers both values and errors into the
     *         success type.
     */
    Task<Either<T,E>,E> materialize() const noexcept  {
        return Task<Either<T,E>,E>(
            op->flatMap([](auto&& fiber_value) {
                if(fiber_value.isValue()) {
                    auto value = fiber_value.underlying().template get<T>();
                    auto either = Either<T,E>::left(std::forward<T>(value));
                    auto erased = Erased(std::move(either));
                    return fiber::FiberOp::value(std::move(erased));
                } else if(fiber_value.isError()) {
                    auto error = fiber_value.underlying().template get<E>();
                    auto either = Either<T,E>::right(std::forward<E>(error));
                    auto erased = Erased(std::move(either));
                    return fiber::FiberOp::value(std::move(erased));
                } else {
                    return fiber::FiberOp::cancel();
                }
            })
        );
    }

    /**
     * Dematerialize the success type which represents both values and errors. 
     * This operation is the inverse of `materialize` and, afterwards, provides
     * a task where normal operations such as `map` and `flatMap` no longer
     * operate on discovered errors.
     *
     * @return A new `Task` which dematerializes the success type containing
     *         both values and errors.
     */
    /*
    template <typename T2, typename std::enable_if<
        std::is_assignable<Either<T2,E>,T>::value
    >::type* = nullptr>
    */
   template <typename T2, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<T>,
            Either<T2,E>
        >::value
    >>
    Task<T2,E> dematerialize() const noexcept  {
        return Task<T2,E>(
            op->flatMap([](auto&& fiber_input) {
                if(fiber_input.isValue()) {
                    auto either = fiber_input.underlying().template get<Either<T2,E>>();
                    if(either.is_left()) {
                        auto value = either.get_left();
                        auto erased = Erased(std::forward<T2>(value));
                        return fiber::FiberOp::value(erased);
                    } else {
                        auto error = either.get_right();
                        auto erased = Erased(std::forward<E>(error));
                        return fiber::FiberOp::error(erased);
                    }
                } else if(fiber_input.isError()) {
                    return fiber::FiberOp::error(fiber_input.underlying());
                } else {
                    return fiber::FiberOp::cancel();
                }
            })
        );
    }

    /**
     * Delay the exeuction of the given tasks by some number of milliseconds.
     * Note that the given delay will be _at least_ the given number of
     * milliseconds but may be more. Do not use these delays for the purposes
     * of generating an accurate time.
     * 
     * @param milliseconds The number of milliseconds to delay the task by.
     * @return A new `Task` represening the delayed execution.
     */
    Task<T,E> delay(uint32_t milliseconds) const noexcept  {
        return Task<T,E>(
            fiber::FiberOp::delay(milliseconds)->flatMap([op = this->op](auto&& result) {
                if(result.isCanceled()) {
                    return fiber::FiberOp::cancel();
                } else {
                    return op;
                }
            })
        );
    }

    /**
     * Recover from an error by transforming it into some success value.
     * 
     * @param predicate The recovery method.
     * @return A new `Task` that will recover from errors.
     */
    template <typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<T(E&&)>
        >::value
    >>
    Task<T,E> recover(Predicate&& predicate) const noexcept  {
        return Task<T,E>(
            op->flatMap([predicate = std::forward<Predicate>(predicate)](auto&& fiber_input) {
                if(fiber_input.isValue()) {
                    return fiber::FiberOp::value(fiber_input.underlying());
                } else if(fiber_input.isError()) {
                    auto error = fiber_input.underlying().template get<E>();
                    auto result = predicate(std::forward<E>(error));
                    auto erased = Erased(std::move(result));
                    return fiber::FiberOp::value(std::move(erased));
                } else {
                    return fiber::FiberOp::cancel();
                }
            })
        );
    }

    /**
     * Restarts this task until the given predicate function returns true.
     * 
     * @param predicate Function to evaluate when deciding if the given
     *                  task should be restarted.
     * @return A new `Task` which restarts as needed.
     */
    template <typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<bool(const T&)>
        >::value
    >>
    Task<T,E> restartUntil(Predicate&& predicate) const noexcept  {
        return Task<T,E>::defer([self = *this, predicate = std::forward<Predicate>(predicate)] {
            return self.template flatMap<T>([self, predicate](auto&& value) {
                if(predicate(value)) {
                    return Task<T,E>::pure(value);
                } else {
                    return self.restartUntil(predicate);
                }
            });
        });
    }

    /**
     * Runs this task and the given other task concurrently and provides
     * the value of whichever tasks finishes first.
     * 
     * @param other The task to race the current task with.
     * @result The value of the task which finished first.
     */
    template <typename Arg, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Arg>,
            Task<T,E>
        >::value
    >>
    Task<T,E> raceWith(Arg&& other) const noexcept {
        return Task<T,E>(
            fiber::FiberOp::race({op, other.op})
        );
    }

    /**
     * Runs the given task as a side effect whose success results are
     * do not effect the output of the original task.
     *
     * @param task The side effecting task.
     * @result A task which mirrors the results of the original task
     *         but with the added evaluation of the given side effect.
     */
    template <typename T2, typename Arg, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Arg>,
            Task<T2,E>
        >::value
    >>
    Task<T,E> sideEffect(Arg&& task) const noexcept  {
        return flatMap<T>([task = std::forward<Arg>(task)](T&& result) {
            return task.template map<T>([result = std::forward<T>(result)](T2&&) {
                return result;
            });
        });
    }

    /**
     * Guarantee that the given task will be run when this task completes
     * regardless of success or error.
     *
     * @param task The task to run on success or error.
     * @result A task which mirrors the results of the original task
     *         but with the added guaranteed evalution of the given task.
     */
    template <typename Arg, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Arg>,
            Task<None,E>
        >::value
    >>
    Task<T,E> guarantee(Arg&& task) const noexcept  {
        return Task<T,E>(
            op->flatMap(
                [task = std::forward<Arg>(task)](auto&& fiber_value) {
                    return task.op->flatMap([fiber_value = std::forward<fiber::FiberValue>(fiber_value)](auto&& guaranteed_value) {
                        if(guaranteed_value.isError()) {
                            return fiber::FiberOp::error(guaranteed_value.underlying());
                        } else if(guaranteed_value.isCanceled() || fiber_value.isCanceled()) {
                            return fiber::FiberOp::cancel();
                        } else if(fiber_value.isValue()) {
                            return fiber::FiberOp::value(fiber_value.underlying());
                        } else {
                            return fiber::FiberOp::error(fiber_value.underlying());
                        }
                    });
                }
            )
        );
    }

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
    template <typename Arg, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Arg>,
            E
        >::value
    >>
    Task<T,E> timeout(uint32_t milliseconds, Arg&& error) const noexcept  {
        auto timeoutTask = Task<T,E>::raiseError(std::forward<Arg>(error)).delay(milliseconds);
        return raceWith(timeoutTask);
    }

    /**
     * Construct a task which wraps the given trampoline operations. This
     * should not be called directly and, instead, users should use provided
     * operators to build these operations automatically.
     */
    explicit Task(const std::shared_ptr<const fiber::FiberOp>& op) noexcept
        : op(op)
    {}

    explicit Task(std::shared_ptr<const fiber::FiberOp>&& op) noexcept
        : op(std::move(op))
    {}

    Task(const Task<T,E>& other) noexcept
        : op(other.op)
    {}

    Task(Task<T,E>&& other) noexcept
        : op(std::move(other.op))
    {}

    Task<T,E>& operator=(const Task<T,E>& other) noexcept {
        if(this != &other) {
            this->op = other.op;
        }
        return *this;
    }

    Task<T,E>& operator=(Task<T,E>&& other) noexcept {
        this->op = std::move(other.op);
        return *this;
    }

    std::shared_ptr<const fiber::FiberOp> op;
};

} // namespace cask

#endif