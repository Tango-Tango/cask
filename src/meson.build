cask_sources = [
    'cask/fiber/FiberOp.cpp',
    'cask/fiber/FiberValue.cpp',
    'cask/Erased.cpp',
    'cask/Scheduler.cpp',
    'cask/scheduler/BenchScheduler.cpp',
    'cask/scheduler/SingleThreadScheduler.cpp',
    'cask/scheduler/ThreadPoolScheduler.cpp'
]

cask_lib = static_library('cask',
    cask_sources,
    include_directories: includes,
    dependencies: dependencies
)

