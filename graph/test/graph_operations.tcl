# Test graph operations with larger multi-clock design for coverage.
# Targets: Graph.cc (makeGraph, makeVerticesAndEdges, makeWireEdges,
#   makePinVertices, makeInstanceEdges, pinVertices, pinDrvrVertex,
#   pinLoadVertex, vertexCount, edgeCount, vertexIterator, edgeIterator,
#   arcDelayCount, hasDownstreamClkPin, regClkVertices, isRegClk,
#   isLatchData, widthCheckAnnotation, periodCheckAnnotation)
# GraphCmp.cc (EdgeLess, sortEdges, VertexNameLess, vertexLess)

source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog graph_test3.v
link_design graph_test3

#---------------------------------------------------------------
# Two clock domains
#---------------------------------------------------------------
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 15 [get_ports clk2]
set_input_delay -clock clk1 1.0 [get_ports {d1 d2 d3 d4}]
set_output_delay -clock clk1 1.0 [get_ports {q1 q3}]
set_output_delay -clock clk2 1.0 [get_ports q2]
set_input_transition 0.1 [get_ports {d1 d2 d3 d4 rst clk1 clk2}]

#---------------------------------------------------------------
# Baseline timing (exercises makeGraph, graph construction)
#---------------------------------------------------------------
puts "--- baseline timing ---"
report_checks

report_checks -path_delay min

report_checks -path_delay max

#---------------------------------------------------------------
# All path combinations (exercises vertex/edge traversal thoroughly)
#---------------------------------------------------------------
puts "--- all path combinations ---"
foreach from_port {d1 d2 d3 d4} {
  foreach to_port {q1 q2 q3} {
    report_checks -from [get_ports $from_port] -to [get_ports $to_port]
    puts "${from_port}->${to_port}: done"
  }
}

#---------------------------------------------------------------
# Through pin queries for reconvergent paths
# Exercises: graph traversal through reconvergent fan-out
#---------------------------------------------------------------
puts "--- through reconvergent paths ---"
report_checks -through [get_pins nand1/ZN]
puts "through nand1/ZN: done"

report_checks -through [get_pins nor1/ZN]
puts "through nor1/ZN: done"

report_checks -through [get_pins and2/ZN]
puts "through and2/ZN: done"

report_checks -through [get_pins or2/ZN]
puts "through or2/ZN: done"

# Through multiple intermediate points
report_checks -through [get_pins and1/ZN] -through [get_pins nand1/ZN]
puts "through and1->nand1: done"

report_checks -through [get_pins or1/ZN] -through [get_pins nand1/ZN]
puts "through or1->nand1: done"

#---------------------------------------------------------------
# Timing edge queries for all cells (exercises edge iteration)
#---------------------------------------------------------------
puts "--- timing edges all cells ---"
foreach cell_name {buf1 buf2 inv1 inv2 and1 or1 nand1 nor1 and2 or2 reg1 reg2 reg3 buf3 buf4} {
  set edges [get_timing_edges -of_objects [get_cells $cell_name]]
  puts "$cell_name edges: [llength $edges]"
}

# From/to specific pins
puts "--- specific edge queries ---"
set edges_and1_a1 [get_timing_edges -from [get_pins and1/A1] -to [get_pins and1/ZN]]
puts "and1 A1->ZN: [llength $edges_and1_a1]"

set edges_and1_a2 [get_timing_edges -from [get_pins and1/A2] -to [get_pins and1/ZN]]
puts "and1 A2->ZN: [llength $edges_and1_a2]"

set edges_or1_a1 [get_timing_edges -from [get_pins or1/A1] -to [get_pins or1/ZN]]
puts "or1 A1->ZN: [llength $edges_or1_a1]"

set edges_nand_a1 [get_timing_edges -from [get_pins nand1/A1] -to [get_pins nand1/ZN]]
puts "nand1 A1->ZN: [llength $edges_nand_a1]"

set edges_nor_a1 [get_timing_edges -from [get_pins nor1/A1] -to [get_pins nor1/ZN]]
puts "nor1 A1->ZN: [llength $edges_nor_a1]"

# DFF edges
set edges_reg1_ck_q [get_timing_edges -from [get_pins reg1/CK] -to [get_pins reg1/Q]]
puts "reg1 CK->Q: [llength $edges_reg1_ck_q]"

set edges_reg3_ck_q [get_timing_edges -from [get_pins reg3/CK] -to [get_pins reg3/Q]]
puts "reg3 CK->Q: [llength $edges_reg3_ck_q]"

#---------------------------------------------------------------
# Report edges for all cell types
#---------------------------------------------------------------
puts "--- report_edges ---"
report_edges -from [get_pins buf1/A] -to [get_pins buf1/Z]

report_edges -from [get_pins inv1/A] -to [get_pins inv1/ZN]

report_edges -from [get_pins and1/A1] -to [get_pins and1/ZN]

report_edges -from [get_pins and1/A2] -to [get_pins and1/ZN]

report_edges -from [get_pins or1/A1] -to [get_pins or1/ZN]

report_edges -from [get_pins nand1/A1] -to [get_pins nand1/ZN]

report_edges -from [get_pins nor1/A1] -to [get_pins nor1/ZN]

report_edges -from [get_pins reg1/CK] -to [get_pins reg1/Q]

report_edges -from [get_pins reg3/CK] -to [get_pins reg3/Q]

# From only
report_edges -from [get_ports d1]

report_edges -from [get_ports d3]

# To only
report_edges -to [get_ports q1]

report_edges -to [get_ports q2]

report_edges -to [get_ports q3]

#---------------------------------------------------------------
# Disable/enable timing on various cells
# Exercises: graph edge disable/enable, re-traversal
#---------------------------------------------------------------
puts "--- disable/enable timing ---"

# Disable individual cells
set_disable_timing [get_cells buf1]
report_checks

set_disable_timing [get_cells inv1]
report_checks

set_disable_timing [get_cells nand1]
report_checks

# Enable back one by one
unset_disable_timing [get_cells buf1]
report_checks

unset_disable_timing [get_cells inv1]
report_checks

unset_disable_timing [get_cells nand1]
report_checks

# Disable specific arcs on lib cells
set_disable_timing -from A1 -to ZN [get_lib_cells NangateOpenCellLibrary/AND2_X1]
report_disabled_edges
report_checks

unset_disable_timing -from A1 -to ZN [get_lib_cells NangateOpenCellLibrary/AND2_X1]
report_disabled_edges
report_checks

# Disable/enable on NOR and NAND
set_disable_timing -from A1 -to ZN [get_lib_cells NangateOpenCellLibrary/NAND2_X1]
report_checks

unset_disable_timing -from A1 -to ZN [get_lib_cells NangateOpenCellLibrary/NAND2_X1]
report_checks

set_disable_timing -from A1 -to ZN [get_lib_cells NangateOpenCellLibrary/NOR2_X1]
report_checks

unset_disable_timing -from A1 -to ZN [get_lib_cells NangateOpenCellLibrary/NOR2_X1]
report_checks

#---------------------------------------------------------------
# Case analysis / constant propagation
# Exercises: graph constant propagation, re-traversal
#---------------------------------------------------------------
puts "--- case analysis ---"
set_case_analysis 1 [get_ports rst]
report_checks

set_case_analysis 0 [get_ports rst]
report_checks

unset_case_analysis [get_ports rst]
report_checks

# Case analysis on data inputs
set_case_analysis 1 [get_ports d3]
report_checks

unset_case_analysis [get_ports d3]
report_checks

#---------------------------------------------------------------
# Report slews for pins in multi-clock design
# Exercises: vertex slew access across corners
#---------------------------------------------------------------
puts "--- report_slews ---"
report_slews [get_ports d1]
report_slews [get_ports d2]
report_slews [get_ports d3]
report_slews [get_ports d4]
report_slews [get_ports q1]
report_slews [get_ports q2]
report_slews [get_ports q3]
report_slews [get_pins buf1/Z]
report_slews [get_pins inv1/ZN]
report_slews [get_pins and1/ZN]
report_slews [get_pins or1/ZN]
report_slews [get_pins nand1/ZN]
report_slews [get_pins nor1/ZN]
report_slews [get_pins and2/ZN]
report_slews [get_pins or2/ZN]
report_slews [get_pins reg1/Q]
report_slews [get_pins reg2/Q]
report_slews [get_pins reg3/Q]

#---------------------------------------------------------------
# report_check_types (exercises check edge categorization)
#---------------------------------------------------------------
puts "--- report_check_types ---"
report_check_types -max_delay -verbose

report_check_types -min_delay -verbose

report_check_types -max_delay -min_delay -verbose

#---------------------------------------------------------------
# report_checks with various options
#---------------------------------------------------------------
puts "--- report_checks options ---"
report_checks -fields {slew cap input_pins nets fanout}

report_checks -format full_clock

report_checks -unconstrained

report_checks -group_count 3

report_checks -endpoint_count 5

report_checks -sort_by_slack

report_checks -endpoint_count 3 -path_delay min
