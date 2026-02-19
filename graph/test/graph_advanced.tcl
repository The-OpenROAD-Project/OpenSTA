# Test advanced graph operations: multiple-instance design, edge traversal,
# and graph info queries.
# Targets uncovered Graph.cc functions: vertex operations, edge iterators,
# constant propagation, level reporting, etc.

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Load a larger design for more graph coverage
#---------------------------------------------------------------
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog graph_test1.v
link_design graph_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports d]
set_output_delay -clock clk 1.0 [get_ports q]
set_input_transition 0.1 [get_ports d]

#---------------------------------------------------------------
# report_checks exercises graph traversal
#---------------------------------------------------------------
puts "--- report_checks baseline ---"
report_checks

puts "--- report_checks -path_delay min ---"
report_checks -path_delay min

puts "--- report_checks -path_delay max ---"
report_checks -path_delay max

puts "--- report_checks -from/-to ---"
report_checks -from [get_ports d] -to [get_ports q]

puts "--- report_checks -through ---"
set rc [catch { report_checks -through [get_pins reg1/Q] } msg]
if { $rc == 0 } {
} else {
  puts "INFO: report_checks -through: $msg"
}

#---------------------------------------------------------------
# Edge queries (Graph.cc edge functions)
#---------------------------------------------------------------
puts "--- get_timing_edges full combinations ---"
set edges_all [get_timing_edges -of_objects [get_cells reg1]]
puts "reg1 all edges: [llength $edges_all]"

set edges_all2 [get_timing_edges -of_objects [get_cells reg2]]
puts "reg2 all edges: [llength $edges_all2]"

puts "--- report_edges for cells ---"
report_edges -from [get_pins reg1/CK] -to [get_pins reg1/Q]

report_edges -from [get_pins reg2/CK] -to [get_pins reg2/Q]

report_edges -from [get_pins reg1/CK]

report_edges -to [get_pins reg2/D]

#---------------------------------------------------------------
# set_disable_timing / report_disabled_edges exercises more paths
#---------------------------------------------------------------
puts "--- disable_timing on port pin ---"
set_disable_timing -from CK -to Q [get_lib_cells NangateOpenCellLibrary/DFF_X1]
report_disabled_edges

report_checks

unset_disable_timing -from CK -to Q [get_lib_cells NangateOpenCellLibrary/DFF_X1]
report_disabled_edges

puts "--- set_disable_timing instance and back ---"
set_disable_timing [get_cells reg1]
report_disabled_edges
report_checks

unset_disable_timing [get_cells reg1]
report_disabled_edges
report_checks

#---------------------------------------------------------------
# Slew reporting (exercises vertex slew access)
#---------------------------------------------------------------
puts "--- report_slews for various pins ---"
report_slews [get_ports d]
report_slews [get_ports q]
report_slews [get_pins reg1/CK]
report_slews [get_pins reg1/Q]
report_slews [get_pins reg2/D]

#---------------------------------------------------------------
# Graph verification
#---------------------------------------------------------------
puts "--- report_check_types ---"
report_check_types -max_delay -min_delay -verbose

puts "--- report_checks with -format ---"
report_checks -format full_clock

puts "--- report_checks -unconstrained ---"
report_checks -unconstrained

#---------------------------------------------------------------
# Additional graph traversals (exercises more vertex/edge paths)
#---------------------------------------------------------------
puts "--- report_checks -group_count 2 ---"
report_checks -group_count 2

puts "--- report_checks -endpoint_count 2 ---"
report_checks -endpoint_count 2
