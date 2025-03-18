# Generate the hooch configuration file
conf_data = configuration_data()
conf_data.set('ref_uses_atomics', get_option('ref_uses_atomics').to_string())
conf_data.set('cede_iterations', get_option('cede_iterations'))
conf_data.set('initial_blocks_per_pool', get_option('initial_blocks_per_pool'))
conf_data.set('cache_line_size', get_option('cache_line_size'))
conf_data.set('work_steal_thread_queue_size', get_option('work_steal_thread_queue_size'))
configure_file(input: 'Config.hpp.in', output: 'Config.hpp', configuration: conf_data, install: true, install_dir: 'include/cask')

# Install headers
install_headers(
    'BufferRef.hpp',
    'Cancelable.hpp',
    'Deferred.hpp',
    'Either.hpp',
    'Erased.hpp',
    'Fiber.hpp',
    'List.hpp',
    'MVar.hpp',
    'None.hpp',
    'Observable.hpp',
    'Observer.hpp',
    'Pool.hpp',
    'Promise.hpp',
    'Queue.hpp',
    'Ref.hpp',
    'Resource.hpp',
    'Scheduler.hpp',
    'Task.hpp',
    subdir: 'cask')

subdir('deferred')
subdir('fiber')
subdir('list')
subdir('mvar')
subdir('observable')
subdir('pool')
subdir('queue')
subdir('scheduler')