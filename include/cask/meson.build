# Generate the hooch configuration file
conf_data = configuration_data()
conf_data.set('ref_uses_atomics', get_option('ref_uses_atomics').to_string())
conf_data.set('batch_size', get_option('batch_size'))
conf_data.set('erased_pool_block_size', get_option('erased_pool_block_size'))
conf_data.set('erased_pool_startup_blocks', get_option('erased_pool_startup_blocks'))
conf_data.set('fiber_op_startup_blocks', get_option('fiber_op_startup_blocks'))
configure_file(input: 'Config.hpp.in', output: 'Config.hpp', configuration: conf_data)