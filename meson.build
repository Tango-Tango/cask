project('cask', 'cpp', version: '0.1.0',default_options: ['cpp_std=c++17'])

cpp = meson.get_compiler('cpp')

# Declare Dependencies
dependencies = [
    subproject('concurrentqueue').get_variable('concurrentqueue_dep')
]

# Setup the doxygen docs task
run_target('docs', command : ['doxygen', meson.source_root() + '/Doxyfile'])

# Setup cppcheck
run_target('cppcheck', command : [
    'cppcheck',
    '-q',
    '--enable=all',
    '--project=' + join_paths(meson.build_root(), 'compile_commands.json'),
    '--suppressions-list=' + join_paths(meson.source_root(), '.cppcheck_suppressions')
]) 

# Setup the 'hooch' project structure
includes = include_directories('include')
subdir('src')
subdir('test')
