if not meson.is_subproject()
    test_dependencies = [
        subproject('gtest').get_variable('gtest_dep'),
        subproject('trompeloeil').get_variable('trompeloeil_dep')
    ]

    test_sources = [
        'cask/TestEither.cpp',
        'cask/TestErased.cpp',
        'cask/TestFiber.cpp',
        'cask/TestList.cpp',
        'cask/TestMVar.cpp',
        'cask/TestObservable.cpp',
        'cask/TestPromiseDeferred.cpp',
        'cask/TestQueue.cpp',
        'cask/TestRef.cpp',
        'cask/TestResource.cpp',
        'cask/TestTask.cpp',
        'cask/mvar/TestMVarState.cpp',
        'cask/observable/TestObservableAppendAll.cpp',
        'cask/observable/TestObservableBuffer.cpp',
        'cask/observable/TestObservableFilter.cpp',
        'cask/observable/TestObservableFlatMap.cpp',
        'cask/observable/TestObservableFlatten.cpp',
        'cask/observable/TestObservableForeach.cpp',
        'cask/observable/TestObservableForeachTask.cpp',
        'cask/observable/TestObservableFromTask.cpp',
        'cask/observable/TestObservableGuarantee.cpp',
        'cask/observable/TestObservableLast.cpp',
        'cask/observable/TestObservableMap.cpp',
        'cask/observable/TestObservableMapBothTask.cpp',
        'cask/observable/TestObservableMapError.cpp',
        'cask/observable/TestObservableMapTask.cpp',
        'cask/observable/TestObservableRepeatTask.cpp',
        'cask/observable/TestObservableSwitchMap.cpp',
        'cask/observable/TestObservableTake.cpp',
        'cask/observable/TestObservableTakeWhile.cpp',
        'cask/observable/TestObservableTakeWhileInclusive.cpp',
        'cask/scheduler/TestBenchScheduler.cpp',
        'cask/scheduler/TestSingleThreadScheduler.cpp',
        'cask/scheduler/TestThreadPoolScheduler.cpp',
        'cask/task/TestTaskAssignment.cpp',
        'cask/task/TestTaskDefer.cpp',
        'cask/task/TestTaskDelay.cpp',
        'cask/task/TestTaskEval.cpp',
        'cask/task/TestTaskFlatMapBoth.cpp',
        'cask/task/TestTaskGuarantee.cpp',
        'cask/task/TestTaskNever.cpp',
        'cask/task/TestTaskNone.cpp',
        'cask/task/TestTaskPure.cpp',
        'cask/task/TestTaskRace.cpp',
        'cask/task/TestTaskRaiseError.cpp',
        'cask/task/TestTaskRecover.cpp',
        'cask/task/TestTaskTimeout.cpp',
        'main.cpp'
    ]

    testexe = executable(
        'testexe',
        test_sources,
        include_directories: includes,
        link_with: cask_lib,
        dependencies: dependencies + test_dependencies
    )

    test('cask unit tests', testexe, timeout: 30, env: ['ASAN_OPTIONS=detect_leaks=1'])
endif
