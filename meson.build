project(
    'cask',
    'cpp',
    default_options: ['cpp_std=c++17'],
    version: files('VERSION'),
    license: 'BSL-1.0',
    meson_version: '>= 1.0.0'
)
cpp = meson.get_compiler('cpp')

# Enable /bigobj on msvc
if cpp.get_id() == 'msvc'
	add_project_arguments('/bigobj', language: 'cpp')
endif

# Discover Number of CPU Threads
num_cpu_threads = 1
bash = find_program('bash', required : false)
if bash.found()
    threads_result = run_command(bash, '-c', 'cat /proc/cpuinfo | grep processor | wc -l', check: false)
    if threads_result.returncode() == 0
        num_cpu_threads = threads_result.stdout().strip()
    endif
endif

# Declare Dependencies
dependencies = []

# Check if libatomic is needed
cxx_atomics_check_code = '''
#include <atomic>
#include <cstdint>
struct Block {
    void* ptr;
    std::uint16_t counter;

    Block() : ptr(nullptr), counter(0) {}
};

int main() {
    Block initial_block;
    std::atomic<Block> head(initial_block);

    Block old_head = head.load();
    Block new_head;
    new_head.ptr = old_head.ptr;
    new_head.counter = old_head.counter + 1;
    head.compare_exchange_weak(old_head, new_head);
	return 0;
}
'''

if not cpp.links(cxx_atomics_check_code, name: 'std::atomic')
    libatomic = cpp.find_library('atomic')
    
    assert(
        cpp.links(cxx_atomics_check_code, dependencies: libatomic, name: 'std::atomic with libatomic'),
        'Cask requires support for C++ std::atomic either builtin or via libatomic'
    )

    dependencies += libatomic
endif

# Setup the doxygen docs task
doxygen = find_program('doxygen', required : false)
if doxygen.found()
    run_target('docs', command : [
        doxygen,
        meson.project_source_root() + '/Doxyfile'
    ])
endif

# Setup cppcheck
cppcheck = find_program('cppcheck', required : false)
if cppcheck.found()
    run_target('cppcheck', command : [
        cppcheck,
        '-q',
        '-j', num_cpu_threads,
        '--enable=all',
        '--project=' + join_paths(meson.project_build_root(), 'compile_commands.json'),
        '--error-exitcode=1'
    ])
endif

# Setup clang-tidy
run_clang_tidy = find_program('run-clang-tidy', required: false)
if run_clang_tidy.found()
    run_target('clang-tidy', command: [
        run_clang_tidy,
        '-config=',
        '-quiet'
    ])
endif

# Setup the 'cask' project structure
subdir('include/cask')
includes = include_directories('include')
subdir('src')
subdir('test')

# Provide a dependency hook for downstream projects
cask_dep = declare_dependency(
    link_with: [cask_lib],
    include_directories: includes
)

pkg = import('pkgconfig')
pkg.generate(cask_lib)
