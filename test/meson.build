if not meson.is_subproject()
    test_dependencies = [
        subproject('gtest').get_variable('gtest_dep'),
        subproject('immer').get_variable('immer_dep')
    ]

    test_sources = [
        'cask/TestEither.cpp',
        'cask/TestMVar.cpp',
        'cask/TestObservable.cpp',
        'cask/TestPromiseDeferred.cpp',
        'cask/TestRef.cpp',
        'cask/TestResource.cpp',
        'cask/TestTask.cpp',
        'cask/TestTrampoline.cpp',
        'main.cpp'
    ]

    testexe = executable(
        'testexe',
        test_sources,
        include_directories: includes,
        link_with: cask_lib,
        dependencies: dependencies + test_dependencies
    )

    test('cask unit tests', testexe, timeout: 60, env: ['ASAN_OPTIONS=detect_leaks=1'])
endif
