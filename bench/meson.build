if get_option('build_benchmarks')
    benchmark_dep = subproject('google-benchmark').get_variable('google_benchmark_dep')

    bench_sources = [
        'BenchErased.cpp',
        'BenchFlatMap.cpp',
        'BenchPool.cpp',
        'main.cpp'
    ]

    benchexe = executable(
        'benchexe',
        bench_sources,
        include_directories: includes,
        link_with: cask_lib,
        dependencies: dependencies + [benchmark_dep]
    )
endif

