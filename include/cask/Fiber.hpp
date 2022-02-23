//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_FIBER_H_
#define _CASK_FIBER_H_

#include "cask/Cancelable.hpp"
#include "cask/Either.hpp"
#include "cask/Scheduler.hpp"
#include "cask/fiber/FiberOp.hpp"

namespace cask {

template <class T, class E>
class Fiber;

template <class T, class E>
using FiberRef = std::shared_ptr<Fiber<T, E>>;

/**
 * A Fiber is a user-space cooperatively scheduled operation that has been started
 * concurrently. It is used to represent a running `Task` or `Observable` whose
 * results may be observed, pended, awaited, and canceled.
 *
 * A canceled fiber may take some time to process the cancelation. The only way
 * to reliably ensure fibers run to completion (either canceled or via normal
 * execution) is to observe their results via `onFiberShutdown` or `await`.
 *
 * Fibers run on a `Scheduler` by default. They can also be run synchronously
 * without a scheduler but some operations which require a scheduler will not
 * be possible and no value will be provided as a result. This is useful when
 * implementing known synchronous operations from "outside" the normal cask world.
 * For example `Queue::tryPut` takes advantage of this behavior to provide
 * non-blocking put or fail semantics to its callers.
 *
 * If a fiber is freed by its caller its execution will simply halt at the
 * next rescheduling opportunity. For a fiber to provide results it is important
 * that callers hold onto the fiber at least until a result of some form is
 * observed. In the case of canecelation this still applies - callers should
 * ensure the fiber provides a canceled result before releasing their hold
 * on the Fiber itself.
 */
template <class T, class E>
class Fiber
    : public Cancelable
    , public std::enable_shared_from_this<Fiber<T, E>> {
public:
    /**
     * Run the given FiberOp on the given scheduler.
     *
     * NOTE: Most users should construct a `Fiber` by using `Task` and `Observable`
     *       to form their computation and then call `run` or `subscribe` to
     *       produce a `Fiber` rather than calling this method directly.
     *
     * @param op The FiberOp to being execution with.
     * @param sched The scheduler to run the Fiber on.
     * @return A Fiber representing the ongoing concurrent execution of the op.
     */
    static FiberRef<T, E> run(const std::shared_ptr<const fiber::FiberOp>& op, const std::shared_ptr<Scheduler>& sched);

    /**
     * Run the given FiberOp without a scheduler.
     *
     * NOTE: Most users should use relevant `Task` and `Observable` methods which
     *       use this method automatically when it is relevant to their use case.
     *
     * @param op The FiberOp to being execution with.
     * @return The success or error result of the oeprations execution. If the op
     *         could not be executed without a scheduler then no result is provided.
     */
    static std::optional<Either<T, E>> runSync(const std::shared_ptr<const fiber::FiberOp>& op);

    /**
     * Get the ID of this Fiber.
     *
     * @return The unique identifier of this fiber.
     */
    virtual int getId() = 0;

    /**
     * Return the `FiberValue` associated with this fiber.
     *
     * NOTE: This value may represent an intermediate value and not the end result of
     *       this fiber's execution. As a result, it should only be called in cases
     *       where the caller is sure that the fiber's execution has halted in success,
     *       error, or cancelation.
     *
     * @return The current raw value of this fiber.
     */
    virtual const fiber::FiberValue& getRawValue() = 0;

    /**
     * Return the value result of this fiber if it is available.
     *
     * @return The value result of this fiber or none if the fiber is executing, the
     *         fiber finished with an error, or the fiber was canceled.
     */
    virtual std::optional<T> getValue() = 0;

    /**
     * Return the error result of this fiber if it is available.
     *
     * @return The error result of this fiber or none if the fiber is executing, the
     *         fiber finished with a success value, or the fiber was canceled.
     */
    virtual std::optional<E> getError() = 0;

    /**
     * Check if this fiber was canceled.
     *
     * @return True iff the fiber has finished execution and the fiber was canceled.
     *         False if the fiber is still executing or if the fiber was not canceled.
     */
    virtual bool isCanceled() = 0;

    /**
     * Register a callback that will be called when the fiber completes execution
     * with success, error, or cancelation.
     *
     * @param callback The callback to execution on fiber completion.
     */
    virtual void onFiberShutdown(const std::function<void(Fiber<T, E>*)>& callback) = 0;

    /**
     * Await the success result of this Fiber. If an error is encountered then the
     * error will be thrown. If the fiber is canceled a std::runtime_exception will
     * be thrown.
     *
     * NOTE: This should only be called in contexts where blocking is safe. It should
     *       never be called from within the context of another cooperatively executing
     *       fiber or the system may deadlock.
     *
     * @return The success value of this fiber.
     */
    virtual T await() = 0;

    /**
     * Map the success or error results of a fiber to new values.
     *
     * @param value_transform The transform to run for value results.
     * @param error_transform the transform to run for error results.
     * @return A fiber with the success and error values transformed
     *         to new values.
     */
    template <class T2, class E2>
    FiberRef<T2, E2> mapBoth(const std::function<T2(const T&)>& value_transform,
                             const std::function<E2(const E&)>& error_transform);
};

} // namespace cask

#include "cask/fiber/FiberImpl.hpp"
#include "cask/fiber/FiberMap.hpp"

namespace cask {

template <class T, class E>
FiberRef<T, E> Fiber<T, E>::run(const std::shared_ptr<const fiber::FiberOp>& op,
                                const std::shared_ptr<Scheduler>& sched) {
    auto fiber = std::make_shared<fiber::FiberImpl<T, E>>(op);
    fiber->reschedule(sched);
    return fiber;
}

template <class T, class E>
std::optional<Either<T, E>> Fiber<T, E>::runSync(const std::shared_ptr<const fiber::FiberOp>& op) {
    auto fiber = std::make_shared<fiber::FiberImpl<T, E>>(op);
    fiber->resumeSync();

    if (auto value_opt = fiber->getValue()) {
        return Either<T, E>::left(*value_opt);
    } else if (auto error_opt = fiber->getError()) {
        return Either<T, E>::right(*error_opt);
    } else {
        return {};
    }
}

template <class T, class E>
template <class T2, class E2>
FiberRef<T2, E2> Fiber<T, E>::mapBoth(const std::function<T2(const T&)>& value_transform,
                                      const std::function<E2(const E&)>& error_transform) {
    return std::make_shared<fiber::FiberMap<T, T2, E, E2>>(this->shared_from_this(), value_transform, error_transform);
}

} // namespace cask

#endif