project(
    'cask',
    'cpp',
    default_options: ['cpp_std=c++17'],
    version: files('VERSION'),
    license: 'BSL-1.0',
    meson_version: '>= 0.57.0'
)


cpp = meson.get_compiler('cpp')

# Declare Dependencies
dependencies = []

# Setup the doxygen docs task
run_target('docs', command : ['doxygen', meson.source_root() + '/Doxyfile'])

# Setup cppcheck
run_target('cppcheck', command : [
    'cppcheck',
    '-q',
    '-j', '4',
    '--enable=all',
    '--project=' + join_paths(meson.build_root(), 'compile_commands.json'),
    '--suppressions-list=' + join_paths(meson.source_root(), '.cppcheck_suppressions')
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
