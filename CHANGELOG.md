# Changelog

All feature additions, significant bug fixes, and API changes will be documented
in this file. This project follows [semantic versioning](https://semver.org/).

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
