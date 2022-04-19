# Generate the hooch configuration file
conf_data = configuration_data()
conf_data.set('ref_uses_atomics', get_option('ref_uses_atomics').to_string())
conf_data.set('batch_size', get_option('batch_size'))
configure_file(input: 'Config.hpp.in', output: 'Config.hpp', configuration: conf_data)