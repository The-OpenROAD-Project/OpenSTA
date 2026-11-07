# Verify write_liberty round-trips pin names containing / and []
source "helpers.tcl"
read_liberty liberty_write_escaped_names.lib
set lib_file [make_result_file "liberty_write_escaped_names.lib"]
sta::write_liberty [get_libs liberty_write_escaped_names] $lib_file
report_file $lib_file
