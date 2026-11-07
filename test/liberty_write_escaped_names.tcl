# Verify write_liberty round-trips pin names containing / and []
read_liberty liberty_write_escaped_names.lib
sta::write_liberty [get_libs liberty_write_escaped_names] results/liberty_write_escaped_names.log
