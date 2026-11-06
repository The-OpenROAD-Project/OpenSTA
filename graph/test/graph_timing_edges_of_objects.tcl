# get_timing_edges -of_objects returns a flat list of edges.
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog graph_test2.v
link_design graph_test2

proc report_edges_list { edges } {
  foreach edge $edges {
    set from [get_full_name [get_property $edge from_pin]]
    set to [get_full_name [get_property $edge to_pin]]
    puts "$from -> $to [get_property $edge sense]"
  }
}

puts "--- -of_objects and1 ---"
set edges [get_timing_edges -of_objects [get_cells and1]]
puts "count: [llength $edges]"
report_edges_list $edges

puts "--- -of_objects {and1 reg1} ---"
set edges [get_timing_edges -of_objects [get_cells {and1 reg1}]]
puts "count: [llength $edges]"
report_edges_list $edges

puts "--- -of_objects -filter ---"
set edges [get_timing_edges -of_objects [get_cells {and1 reg1}] \
             -filter "sense == positive_unate"]
puts "count: [llength $edges]"
report_edges_list $edges

puts "--- -of_objects lib cells ---"
set arc_sets [get_timing_edges -of_objects [get_lib_cells {*/AND2_X1 */DFF_X1}]]
puts "count: [llength $arc_sets]"

puts "--- set_disable_timing -of_objects edges ---"
set_disable_timing [get_timing_edges -of_objects [get_cells and1]]
report_disabled_edges
unset_disable_timing [get_timing_edges -of_objects [get_cells and1]]
report_disabled_edges

puts "--- -of_objects with -to ---"
catch {get_timing_edges -of_objects [get_cells and1] -to [get_pins and1/ZN]} msg
puts $msg
