# Changelog

All feature additions, significant bug fixes, and API changes will be documented
in this file. This project follows [semantic versioning](https://semver.org/).

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
