source ../../test/helpers.tcl

read_liberty liberty_ecsm.lib

set cell_names {}
foreach cell [get_lib_cells test_ecsm_lib/*] {
  lappend cell_names [get_name $cell]
}
set cell_names [lsort $cell_names]
puts "ecsm cell count: [llength $cell_names]"
foreach cell_name $cell_names {
  puts "ecsm cell: $cell_name"
}
