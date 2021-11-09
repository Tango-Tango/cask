cask_sources = [
    'cask/FiberOp.cpp',
    'cask/Scheduler.cpp',
    'cask/TrampolineOp.cpp',
    'cask/TrampolineRunLoop.cpp',
    'cask/scheduler/BenchScheduler.cpp',
    'cask/scheduler/SingleThreadScheduler.cpp',
    'cask/scheduler/ThreadPoolScheduler.cpp'
]

cask_lib = static_library('cask',
    cask_sources,
    include_directories: includes,
    dependencies: dependencies
)

