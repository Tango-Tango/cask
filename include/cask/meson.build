# Generate the hooch configuration file
conf_data = configuration_data()
conf_data.set('ref_uses_atomics', get_option('ref_uses_atomics').to_string())
configure_file(input: 'Config.hpp.in', output: 'Config.hpp', configuration: conf_data)