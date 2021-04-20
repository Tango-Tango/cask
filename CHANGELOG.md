# Changelog

All feature additions, significant bug fixes, and API changes will be documented
in this file. This project follows [semantic versioning](https://semver.org/).

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
