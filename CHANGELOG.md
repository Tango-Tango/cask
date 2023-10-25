# Changelog

All feature additions, significant bug fixes, and API changes will be documented
in this file. This project follows [semantic versioning](https://semver.org/).

## 17.1

- Add the ability to set a queue overflow strategy on queue observers. The default
  behavior is to backpressure upstream, which matches the previous behavior.

## 17.0

- Change many operators, contructors, and passed predicate functions to allow value moves.
- Change `Observable::onNext` and `Observable::onError` to accept moves.
- Add the `Observable::sequence` operator.
- Add the `[[nodiscard]]` attribute to methods that return a `FiberRef`.
- Fix `Either::left` and `Either::right` to properly accept moves.
- Fix `FiberValue` to accept moves in its many contructor methods.

## 16.1

- Add `Observable::queue` which creates and observable where upstream and downstream
  run concurrently and are seperated by a queue of a given maximum size.

## 16.0

- Make `Fiber::run` and `Task::run` execute as much of the given task synchronously
  on the current thread for as long as possible. This both increases efficiency (by
  avoiding touch the scheduler as much as possible) and resolves issues with cancelations
  where they wouldn't be properly processed because the fiber run loop was not properly
  "primed"
- Adjust `Task::asyncBoundary` to be a guaranteed cede to the scheduler. Previously this
  boundary could be optimized away by the fiber run loop.
- Add the `CEDE` instruction to `FiberOp` to support guaranteed ceding to the scheduler.

## 15.1

- Add `Observable::cancel` which allows a user to create an observable which will always
  cancel downstream when evaluating.

## 15.0

- Add the `merge`, `mergeAll`, `flatScan`, `scan` and `scanTask` operators to `Observable`.
- `Fiber` ids are now guaranteed to be globally unique. The id of the current fiber can
  be obtained via `Fiber::currentFibeId`.
- `Observer` instances no longer automatically inherit from `std::enable_shared_from_this`.
  Child classes may still opt-in to this behavior (by inheriting directly) on an as-needed
  basis.

## 14.0

- Allow the pool cache line size to be directly configured rather than guessed via
  the `cache_line_size` configure parameter.
- Allow the initial size of the internal block pools to be directly configured via
  the `initial_blocks_per_pool` configure parameter.
- Change the global internal pool to use `std::shared_ptr` to safely track its allocations
  and remove issues at shutdown time due to the previously undefined behavior of the pool's
  static destruction.

## 13.1

- Add the `distinctUntilChanged` operator to `Observable`.
- Add the `distinctUntilChangedBy` operator to `Observable`.

## 13.0

- Update the `Pool` to solve various bugs, to allocate into block sizes based on
  best fit for ag iven type, and to allocate memory in large chunks for better
  performance.
- Add a global pool that is used internally for `Erased` and `FiberOp`.
- Remove `constexpr` from various places where it had no effect / was confusing.
- Remove move constructors from `Erased` to clarify behavior for the compiler.

## 12.0

- Add the `Pool` type as a simple memory pool.
- Add pooling to `Erased` and `FiberOp` object creations.
- Add configuration options to control the pool block sizes and initial
  pool sizes.

## 11.4

- Allow a thread priority to be set for the single threaded scheduler.

## 11.3

- Add the `Observable::flatMapOptional` operator to speed up flows which simply
  want to provide value or nothing downstream.
- Add the `ref_uses_atomics` configuration flag. This provides a configurable
  speedup on platforms which have poor support for `std::atomic` (such as some
  MIPS platforms).

## 11.2

- Add the `Queue::tryTake` method to allow for atomic takes on the queue without
  pending in the scheduler.

## 11.1

- Add the `Observable::never` operator.
- Add the `Observable::concat` operator as an alias for `Observable::appendAll`.

## 11.0

- Add `Fiber` as a replacement for the `TrampolineRunLoop`.
- Add the `forFiber` method to `Deferred`.
- Add the `modify` method to `MVar`.
- Add the `onCancel` method to the `Observer` contract.
- Add the `deferFiber` and `doOnCancel` methods to `Task`.
- Modify the `Observable::subscribe` and `Observable::subscribeHandlers` methods to return a `Fiber`.
- Modify the `Task::run` method to return a `Fiber`.
- Modify the `Task::runSync` method to return either a result or no value.
- Modify the `Task::raceWith` method to not accept an extra type parameter for the raced task.
- Remove the `TrampolineRunLoop` and `TrampolineOp` classes that have been replaced with `Fiber`,
  `FiberOp` and `FiberValue`.
- Remove the `chainDownstream` and `chainDownstreamAsync` methods from `Deferred`.
- Remove the `runCancelableThenPromise` method from `Task`.
- Remove mutex blocking of the cancel callback in `guarantee`.
- Remove the `Deferred:chainDownstream` and `Deferred:chainDownstreamAsync` methods
  which could lead memory.
- Resolve a memory leak resulting from an infinite series of async operations.

## 10.2

- Add the `runCancelableThenPromise` method to `Task`.
- Add the `onShutdown` method to `Cancelable`.
- Add the `forCancelable` method to `Deferred`.
- Fix an issue where the ordering and evaluation of guaranteed effects on an observable
  did not happen in a deterministic manner when using operators which transformed an
  `Observable` into a `Task` (`take`, `last`, `foreach`, and `foreachTask`). Now, any
  guaranteed effects are guaranteed to be evaluated _before_ a result can be observered.

## 10.1

- Add the `appendAll` operator to `Observable`.

## 10.0

- Add the `SingleThreadScheduler` as a more optimized scheduler that runs on
  a single thread.
- Adjust `TrampolineOp` and its methods to return a `const TrampolineOp` in
  more situations - making composing certain kinds of operations easier.

## 9.0

- Allow scheduled timers to be canceled by returning a `Cancelable` from
  `Scheduler::submitAfter`.
- Update `Task::timeout` and `Task::delay` to properly cancel timers when
  the task is canceled to prevent them from leaking.

## 8.0

- Make the `Scheduler` a virtual interface that may be implemented in several ways.
- Move the current `Scheduler` implementation into the `ThreadPoolScheduler`.
- Add the `BenchScheduler` as a scheduler implementation that allows test
  benches to do things like manually advance time.

## 7.6

- Add the `Observable::switchMap` method.

## 7.5

- Add the `Observable::foreach` and `Observable::foreachTask` methods.

## 7.4

- Add the `Task::timeout` method to make it easy to time-bound asynchronous operations
  with safe cancelling behavior.

## 7.3

- Make `Scheduler` evaluate timers far less often (every 10ms rather than 1ms).
  Enhance the timer handler to better deal with scew where the OS does not
  schedule the timer thread on exact scheduled boundaries.
- Add `Scheduler::submitBulk` as an optimization for clients which wawnt to
  submit several items to the scheduler in a single operation.
- Add `Scheduler::isIdle` to allow users to check if the scheduler is currently
  idle (that is - not executing tasks and nothing in the ready queue).

## 7.2

- Add the `Queue` type as a generalizaton of `MVar` but for mailboxes
  of a size larger than 1.

## 7.1

- Add the `tryPut` operation to MVar to allow for a synchronous put which is
  useful when trying to push data into a cask application from another
  async framework.

## 7.0

- Update the `List` API to properly communicate const guarantees.
- Update all `List` operations to be stack safe. Previously, some operations
  (such as `append`) occurred on the stack and could cause stack overflows
  if they were executed on large lists.
- Add the `size()` operation to list which allows users to check the size of
  the list without counting the elements.

## 6.0

- Add the `Erased` type which acts as the "running with scissors" equivalent
  to `std::any`. It avoids checking type id information during casting which
  speeds up the `get` operation - but also means it is rather unsafe. It expects
  that a higher level entity (e.g. `Task`) is capturing this type safety on
  its behalf.
- Update `TrampolineOp` and `TrampolineRunLoop` to use `Erased` rather than
  `std::any`. While this doesn't effect consumers directly (it is hidden
  behind the type-safe `Task` and `Observable` APIs) in most cases -
  it is a breaking API change to that portion of cask. The performance of
  type erasure has a _significant_ impact on the performance of cask and this
  change allows us to optimize this behavior as needed for cask itself.

## 5.0

- Update the `Task` and `Observable` APIs to explicitly use const refernces in
  as many places as possible to reduce overhead.

## 4.0

- Update `MVar` to remove the use of `std::mutex` and use `Ref` instead. In
  several common cases (putting a value when the mvar is empty, or taking
  a value when it is full) `MVar` operations can now be resolved without
  creating any asynchronous boundaries.
- Resolve issues with `MVar` sometimes hanging during unit test.
- Update the `MVar` API to take a scheduler during construction. This
  scheduler will be used to schedule any asynchronous takes or puts.
- Add the `dropWhile` operator to `List`.
- Add the `forPromise` operator to `Task`.

## 3.1

- Add the `List` data structure which is a simple immutable and persistent list
  that supports O(1) head and tail access, O(1) prepend and O(n) append.

## 3.0

- Update the `Observer` interface so that `onNext`, `onComplete` and `onError`
  each return a `Task`. This not only makes these effects easier to reason
  about - but it addresses a severe performance issue where async boundaries
  were being inserted all over the place when running an observable pipeline.
  By using `Task` these effects can either be sychronous or asynchronous without
  requiring further complication of the `Observer` interface.
- `Observer::onNext` no longer allows observers to convey errors of type `E`
  upstream. This clarifies the contract between `Observable` and `Observer`
  such that (a) errors travel from upstream to downstream and (b) when an
  `onNext` call encounters an error the proper default upstream signal is
  to feed a `Stop` back rather than the error.
- Change `Observable::guarantee` to take a `Task<None,None>` to fully clarify
  that both errors and values of the guaranteed task will be ignored - the given
  task will be executed as a pure side-effect.
- Add the `Observable::mapBothTask` operator which allows users to simultaneously
  transform both success and error values.
- Add the `Task::flatMapBoth` operator which allows user to simulataneously transform
  both success and error values.

## 2.4

- Add the `buffer` method to `Observable`.

## 2.3

- Add the `filter` method to `Observable`.

## 2.2

- Add the `recover` method to `Task`.
- Correctly convey errors downstream in `Observable::flatMap` and resolve issues
  with using `Observable::mapError`.

## 2.1

- Add the `guarantee` method to `Observable`.

## 2.0

- Make cancels value-less rather than requiring a value of the error type. With
  this requirement working with errors (e.g. writing transforms) was difficult
  because it implied that error transforms must be _bi-directional_ since cancels
  flow in the reverse direction of normal errors. This would result in users
  being forced to provide error transforms for both for errors being communicated
  downstream (e.g. A -> B) and for cancels being communicated upstream
  (e.g. B -> A).

## 1.5

- Add the `runSync` method to `Task` which attempts to run a task synchronously
  but stops when an async boundary is encountered. Add the `executeSync` and
  `executeAsyncBoundary` methods to `TrampolineRunLoop` to support this. Add
  the `AsyncBoundary` type to represent when the trampoline has encountered
  such a boundary during synchronous execution.

## 1.4

- Add `flatten` and `fromTask` to `Observable`.

## 1.3

- Add the `takeWhileInclusive` method to `Observable`.

## 1.2

- Add the `mapError` method to `Observable`.

## 1.1

- Add the `fromVector` and `completed` methods to `Observable`.

## 1.0

- Initial release of the cask library.
