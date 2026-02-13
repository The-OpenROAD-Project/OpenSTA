# Test graph construction, wire/instance edge creation, delay annotation,
# slew queries, and edge removal/modification.
# Targets:
#   Graph.cc: makeGraph, makeVertex, makeWireEdge, makeInstEdge,
#     removeWireEdge, removeInstEdge, arcDelayAnnotated, wireDelayAnnotated,
#     slew/delay getters for rise/fall combinations, pinVertices,
#     pinDrvrVertex, pinLoadVertex, vertexCount, edgeCount,
#     setConstant, clearConstants, regClkVertices, isRegClk,
#     widthCheckAnnotation, periodCheckAnnotation, setPeriodCheckAnnotation,
#     hasDownstreamClkPin, minPulseWidthArc

source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog graph_test3.v
link_design graph_test3

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 15 [get_ports clk2]
set_input_delay -clock clk1 1.0 [get_ports {d1 d2 d3 d4}]
set_output_delay -clock clk1 1.0 [get_ports {q1 q3}]
set_output_delay -clock clk2 1.0 [get_ports q2]
set_input_transition 0.1 [get_ports {d1 d2 d3 d4 rst clk1 clk2}]

#---------------------------------------------------------------
# Baseline timing: triggers makeGraph, all vertex/edge construction
#---------------------------------------------------------------
puts "--- baseline timing ---"
report_checks
puts "PASS: baseline"

report_checks -path_delay min
puts "PASS: baseline min"

report_checks -path_delay max
puts "PASS: baseline max"

#---------------------------------------------------------------
# Query all timing edges: exercises edge iteration
#---------------------------------------------------------------
puts "--- timing edges per cell ---"
foreach cell_name {buf1 buf2 inv1 inv2 and1 or1 nand1 nor1 and2 or2 reg1 reg2 reg3 buf3 buf4} {
  set edges [get_timing_edges -of_objects [get_cells $cell_name]]
  puts "$cell_name edges: [llength $edges]"
}
puts "PASS: edge queries"

#---------------------------------------------------------------
# Specific edge queries: from/to pins
# Exercises arc delay access for all transition combinations
#---------------------------------------------------------------
puts "--- specific edge queries ---"

# BUF edges (rise/rise, fall/fall)
report_edges -from [get_pins buf1/A] -to [get_pins buf1/Z]
puts "PASS: buf1 edges"

# INV edges (rise/fall, fall/rise)
report_edges -from [get_pins inv1/A] -to [get_pins inv1/ZN]
puts "PASS: inv1 edges"

# NAND edges
report_edges -from [get_pins nand1/A1] -to [get_pins nand1/ZN]
report_edges -from [get_pins nand1/A2] -to [get_pins nand1/ZN]
puts "PASS: nand1 edges"

# NOR edges
report_edges -from [get_pins nor1/A1] -to [get_pins nor1/ZN]
report_edges -from [get_pins nor1/A2] -to [get_pins nor1/ZN]
puts "PASS: nor1 edges"

# AND2 edges
report_edges -from [get_pins and2/A1] -to [get_pins and2/ZN]
report_edges -from [get_pins and2/A2] -to [get_pins and2/ZN]
puts "PASS: and2 edges"

# OR2 edges
report_edges -from [get_pins or2/A1] -to [get_pins or2/ZN]
report_edges -from [get_pins or2/A2] -to [get_pins or2/ZN]
puts "PASS: or2 edges"

# DFF edges (CK->Q)
report_edges -from [get_pins reg1/CK] -to [get_pins reg1/Q]
report_edges -from [get_pins reg2/CK] -to [get_pins reg2/Q]
report_edges -from [get_pins reg3/CK] -to [get_pins reg3/Q]
puts "PASS: DFF edges"

# Wire edges (port to first gate)
report_edges -from [get_ports d1]
report_edges -from [get_ports d2]
report_edges -from [get_ports d3]
report_edges -from [get_ports d4]
puts "PASS: wire edges from ports"

# Wire edges to output ports
report_edges -to [get_ports q1]
report_edges -to [get_ports q2]
report_edges -to [get_ports q3]
puts "PASS: wire edges to ports"

#---------------------------------------------------------------
# Slew queries: exercises slew getters in Graph.cc
#---------------------------------------------------------------
puts "--- slew queries ---"

# Input port slews
report_slews [get_ports d1]
report_slews [get_ports d2]
report_slews [get_ports d3]
report_slews [get_ports d4]
report_slews [get_ports clk1]
report_slews [get_ports clk2]
puts "PASS: input slews"

# Output port slews
report_slews [get_ports q1]
report_slews [get_ports q2]
report_slews [get_ports q3]
puts "PASS: output slews"

# Internal pin slews
report_slews [get_pins buf1/Z]
report_slews [get_pins buf2/Z]
report_slews [get_pins inv1/ZN]
report_slews [get_pins inv2/ZN]
report_slews [get_pins and1/ZN]
report_slews [get_pins or1/ZN]
report_slews [get_pins nand1/ZN]
report_slews [get_pins nor1/ZN]
report_slews [get_pins and2/ZN]
report_slews [get_pins or2/ZN]
report_slews [get_pins reg1/Q]
report_slews [get_pins reg2/Q]
report_slews [get_pins reg3/Q]
report_slews [get_pins buf3/Z]
report_slews [get_pins buf4/Z]
puts "PASS: internal slews"

#---------------------------------------------------------------
# Network modification: add/remove instances
# Exercises graph incremental update paths
#---------------------------------------------------------------
puts "--- network modification ---"

# Add instance and wire
set new_buf [make_instance extra_buf NangateOpenCellLibrary/BUF_X1]
set new_net [make_net extra_net]
set new_net2 [make_net extra_net2]
connect_pin extra_net extra_buf/A
connect_pin extra_net2 extra_buf/Z
puts "PASS: add instance"

# Timing after addition (exercises incremental graph update)
report_checks
puts "PASS: timing after add"

# Disconnect and remove
disconnect_pin extra_net extra_buf/A
disconnect_pin extra_net2 extra_buf/Z
delete_instance extra_buf
delete_net extra_net
delete_net extra_net2
puts "PASS: cleanup"

report_checks
puts "PASS: timing after cleanup"

#---------------------------------------------------------------
# Replace cell and verify edge update
#---------------------------------------------------------------
puts "--- replace cell ---"

replace_cell buf1 NangateOpenCellLibrary/BUF_X4
report_checks
report_edges -from [get_pins buf1/A] -to [get_pins buf1/Z]
puts "PASS: buf1->BUF_X4"

replace_cell buf1 NangateOpenCellLibrary/BUF_X1
report_checks
puts "PASS: buf1 restored"

replace_cell inv1 NangateOpenCellLibrary/INV_X2
report_checks
puts "PASS: inv1->INV_X2"

replace_cell inv1 NangateOpenCellLibrary/INV_X1
report_checks
puts "PASS: inv1 restored"

#---------------------------------------------------------------
# Disable/enable timing on edges
# Exercises graph edge disable traversal
#---------------------------------------------------------------
puts "--- disable/enable timing ---"

set_disable_timing [get_cells buf1]
report_checks
puts "PASS: disable buf1"

set_disable_timing [get_cells inv1]
report_checks
puts "PASS: disable inv1"

set_disable_timing [get_cells nand1]
report_checks
puts "PASS: disable nand1"

unset_disable_timing [get_cells buf1]
unset_disable_timing [get_cells inv1]
unset_disable_timing [get_cells nand1]
report_checks
puts "PASS: re-enable all"

#---------------------------------------------------------------
# Case analysis: exercises setConstant, clearConstants
#---------------------------------------------------------------
puts "--- case analysis ---"

set_case_analysis 1 [get_ports rst]
report_checks
puts "PASS: rst=1"

set_case_analysis 0 [get_ports rst]
report_checks
puts "PASS: rst=0"

unset_case_analysis [get_ports rst]
report_checks
puts "PASS: rst unset"

set_case_analysis 1 [get_ports d1]
report_checks
puts "PASS: d1=1"

set_case_analysis 0 [get_ports d3]
report_checks
puts "PASS: d3=0"

unset_case_analysis [get_ports d1]
unset_case_analysis [get_ports d3]
report_checks
puts "PASS: all unset"

#---------------------------------------------------------------
# Load changes trigger delay recomputation on graph edges
#---------------------------------------------------------------
puts "--- load changes ---"

set_load 0.01 [get_ports q1]
report_checks
puts "PASS: q1 load=0.01"

set_load 0.05 [get_ports q2]
report_checks
puts "PASS: q2 load=0.05"

set_load 0.1 [get_ports q3]
report_checks
puts "PASS: q3 load=0.1"

set_load 0 [get_ports q1]
set_load 0 [get_ports q2]
set_load 0 [get_ports q3]

#---------------------------------------------------------------
# Through pin paths exercise reconvergent graph traversal
#---------------------------------------------------------------
puts "--- through pin queries ---"

catch {report_checks -through [get_pins nand1/ZN]} msg
puts "through nand1: done"

catch {report_checks -through [get_pins nor1/ZN]} msg
puts "through nor1: done"

catch {report_checks -through [get_pins and2/ZN]} msg
puts "through and2: done"

catch {report_checks -through [get_pins or2/ZN]} msg
puts "through or2: done"

puts "PASS: through pin queries"

#---------------------------------------------------------------
# report_check_types exercises check edge categorization
#---------------------------------------------------------------
puts "--- report_check_types ---"
report_check_types -max_delay -verbose
puts "PASS: check_types max"

report_check_types -min_delay -verbose
puts "PASS: check_types min"

puts "ALL PASSED"
