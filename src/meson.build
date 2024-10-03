cask_sources = [
    'cask/Scheduler.cpp',
    'cask/fiber/CurrentFiber.cpp',
    'cask/fiber/FiberOp.cpp',
    'cask/fiber/FiberValue.cpp',
    'cask/pool/InternalPool.cpp',
    'cask/scheduler/BenchScheduler.cpp',
    'cask/scheduler/SingleThreadScheduler.cpp',
    'cask/scheduler/ThreadPoolScheduler.cpp',
    'cask/scheduler/WorkStealingScheduler.cpp'
]

cask_lib = static_library('cask',
    cask_sources,
    include_directories: includes,
    dependencies: dependencies,
    install: true
)

