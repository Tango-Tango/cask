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
run_target('docs', command : ['doxygen', meson.source_root() + '/Doxyfile'])

# Setup cppcheck
run_target('cppcheck', command : [
    'cppcheck',
    '-q',
    '-j', num_cpu_threads,
    '--enable=all',
    '--project=' + join_paths(meson.build_root(), 'compile_commands.json'),
    '--error-exitcode=1'
])

# Setup the 'cask' project structure
includes = include_directories('include')
subdir('src')
subdir('test')

# Provide a dependency hook for downstream projects
cask_dep = declare_dependency(
    link_with: [cask_lib],
    include_directories: includes
)
