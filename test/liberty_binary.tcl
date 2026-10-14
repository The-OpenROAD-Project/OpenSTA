# write_liberty_binary/read_liberty .blib round trip
source helpers.tcl
set blib_file [make_result_file liberty_binary.blib]
write_liberty_binary liberty_float_as_str.lib $blib_file
read_liberty $blib_file
report_units
puts "cells: [llength [get_lib_cells *]]"
