project(
    'cask',
    'cpp',
    default_options: ['cpp_std=c++17'],
    version: files('VERSION'),
    license: 'BSL-1.0',
    meson_version: '>= 0.57.0'
)


cpp = meson.get_compiler('cpp')

# Discover Number of CPU Threads
num_cpu_threads = 1
threads_result = run_command('/bin/bash', '-c', 'cat /proc/cpuinfo | grep processor | wc -l')
if threads_result.returncode() == 0
    num_cpu_threads = threads_result.stdout().strip()
endif

# Declare Dependencies
dependencies = []

# Setup the doxygen docs task
doxygen = find_program('doxygen', required : false)
if doxygen.found()
    run_target('docs', command : [
        doxygen.full_path(),
        meson.project_source_root() + '/Doxyfile'
    ])
endif

# Setup cppcheck
cppcheck = find_program('cppcheck', required : false)
if cppcheck.found()
    run_target('cppcheck', command : [
        cppcheck.full_path(),
        '-q',
        '-j', num_cpu_threads,
        '--enable=all',
        '--project=' + join_paths(meson.project_build_root(), 'compile_commands.json'),
        '--error-exitcode=1'
    ])
endif

# Setup the 'cask' project structure
includes = include_directories('include')
subdir('src')
subdir('test')

# Provide a dependency hook for downstream projects
cask_dep = declare_dependency(
    link_with: [cask_lib],
    include_directories: includes
)
