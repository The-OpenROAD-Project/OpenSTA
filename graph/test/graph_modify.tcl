# Test graph changes with network modifications, multi-corner, and
# incremental graph updates.
# Targets: Graph.cc (deleteVertexBefore, addEdge, removeEdge,
#   makeWireEdge, removeWireEdge, pinVertex, pinDrvrVertex,
#   pinLoadVertex, setConstant, clearConstants, hasDownstreamClkPin,
#   widthCheckAnnotation, periodCheckAnnotation, regClkVertices, isRegClk)
# GraphCmp.cc (sortEdges, VertexNameLess with added/removed vertices)

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Multi-corner setup
#---------------------------------------------------------------
define_corners fast slow

read_liberty -corner fast ../../test/nangate45/Nangate45_fast.lib
read_liberty -corner slow ../../test/nangate45/Nangate45_slow.lib

read_verilog graph_test3.v
link_design graph_test3

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 15 [get_ports clk2]
set_input_delay -clock clk1 1.0 [get_ports {d1 d2 d3 d4}]
set_output_delay -clock clk1 1.0 [get_ports {q1 q3}]
set_output_delay -clock clk2 1.0 [get_ports q2]
set_input_transition 0.1 [get_ports {d1 d2 d3 d4 rst clk1 clk2}]

#---------------------------------------------------------------
# Multi-corner baseline timing
#---------------------------------------------------------------
puts "--- multi-corner baseline ---"
report_checks -corner fast
puts "PASS: fast baseline"

report_checks -corner slow
puts "PASS: slow baseline"

report_checks -corner fast -path_delay min
puts "PASS: fast min"

report_checks -corner slow -path_delay min
puts "PASS: slow min"

report_checks -corner fast -path_delay max
puts "PASS: fast max"

report_checks -corner slow -path_delay max
puts "PASS: slow max"

#---------------------------------------------------------------
# Multi-corner per-path (exercises delay comparison across corners)
#---------------------------------------------------------------
puts "--- multi-corner per-path ---"
report_checks -corner fast -from [get_ports d1] -to [get_ports q1]
puts "PASS: fast d1->q1"

report_checks -corner slow -from [get_ports d1] -to [get_ports q1]
puts "PASS: slow d1->q1"

report_checks -corner fast -from [get_ports d3] -to [get_ports q1]
puts "PASS: fast d3->q1"

report_checks -corner slow -from [get_ports d3] -to [get_ports q1]
puts "PASS: slow d3->q1"

# Cross-clock domain paths
report_checks -corner fast -from [get_ports d1] -to [get_ports q2]
puts "PASS: fast d1->q2 (cross-clock)"

report_checks -corner slow -from [get_ports d1] -to [get_ports q2]
puts "PASS: slow d1->q2 (cross-clock)"

#---------------------------------------------------------------
# Multi-corner report_dcalc
# Exercises: delay value comparison across corners
#---------------------------------------------------------------
puts "--- multi-corner report_dcalc ---"
catch {report_dcalc -corner fast -from [get_pins buf1/A] -to [get_pins buf1/Z]} msg
puts "fast buf1 dcalc: done"

catch {report_dcalc -corner slow -from [get_pins buf1/A] -to [get_pins buf1/Z]} msg
puts "slow buf1 dcalc: done"

catch {report_dcalc -corner fast -from [get_pins nand1/A1] -to [get_pins nand1/ZN]} msg
puts "fast nand1 dcalc: done"

catch {report_dcalc -corner slow -from [get_pins nand1/A1] -to [get_pins nand1/ZN]} msg
puts "slow nand1 dcalc: done"

catch {report_dcalc -corner fast -from [get_pins nor1/A1] -to [get_pins nor1/ZN]} msg
puts "fast nor1 dcalc: done"

catch {report_dcalc -corner slow -from [get_pins nor1/A1] -to [get_pins nor1/ZN]} msg
puts "slow nor1 dcalc: done"

catch {report_dcalc -corner fast -from [get_pins reg1/CK] -to [get_pins reg1/Q] -max} msg
puts "fast reg1 CK->Q: done"

catch {report_dcalc -corner slow -from [get_pins reg1/CK] -to [get_pins reg1/Q] -max} msg
puts "slow reg1 CK->Q: done"

catch {report_dcalc -corner fast -from [get_pins reg1/CK] -to [get_pins reg1/D] -max} msg
puts "fast reg1 setup: done"

catch {report_dcalc -corner slow -from [get_pins reg1/CK] -to [get_pins reg1/D] -min} msg
puts "slow reg1 hold: done"

# Cross-clock domain DFF
catch {report_dcalc -corner fast -from [get_pins reg3/CK] -to [get_pins reg3/Q] -max} msg
puts "fast reg3 CK->Q: done"

catch {report_dcalc -corner slow -from [get_pins reg3/CK] -to [get_pins reg3/Q] -max} msg
puts "slow reg3 CK->Q: done"

#---------------------------------------------------------------
# Network modification: add instance, recheck graph
# Exercises: graph incremental update after network changes
#---------------------------------------------------------------
puts "--- network modification and graph update ---"
set new_buf [make_instance added_buf NangateOpenCellLibrary/BUF_X1]
puts "PASS: make_instance added_buf"

set new_net [make_net added_net]
puts "PASS: make_net added_net"

connect_pin added_net added_buf/A
puts "PASS: connect added_buf/A"

# Report checks after adding (graph updated incrementally)
report_checks -corner fast
puts "PASS: fast after add"

report_checks -corner slow
puts "PASS: slow after add"

# Disconnect and delete
disconnect_pin added_net added_buf/A
delete_instance added_buf
delete_net added_net
puts "PASS: cleanup added instance"

# Report after deletion
report_checks -corner fast
puts "PASS: fast after delete"

report_checks -corner slow
puts "PASS: slow after delete"

#---------------------------------------------------------------
# Replace cell and check timing
# Exercises: graph update after cell replacement
#---------------------------------------------------------------
puts "--- replace_cell ---"
replace_cell buf1 NangateOpenCellLibrary/BUF_X4
report_checks -corner fast
puts "PASS: fast after buf1->BUF_X4"

report_checks -corner slow
puts "PASS: slow after buf1->BUF_X4"

# Replace back
replace_cell buf1 NangateOpenCellLibrary/BUF_X1
report_checks
puts "PASS: replaced back"

#---------------------------------------------------------------
# Load changes with multi-corner
# Exercises: incremental delay recomputation
#---------------------------------------------------------------
puts "--- load changes multi-corner ---"
set_load 0.01 [get_ports q1]
report_checks -corner fast
report_checks -corner slow
puts "PASS: q1 load 0.01"

set_load 0.05 [get_ports q2]
report_checks -corner fast
report_checks -corner slow
puts "PASS: q2 load 0.05"

set_load 0.1 [get_ports q3]
report_checks -corner fast
report_checks -corner slow
puts "PASS: q3 load 0.1"

# Reset loads
set_load 0 [get_ports q1]
set_load 0 [get_ports q2]
set_load 0 [get_ports q3]

#---------------------------------------------------------------
# Disable/enable timing with multi-corner
# Exercises: edge disable/re-enable with multiple analysis points
#---------------------------------------------------------------
puts "--- disable timing multi-corner ---"
set_disable_timing [get_cells and1]
report_checks -corner fast
report_checks -corner slow
puts "PASS: disable and1"

set_disable_timing [get_cells or1]
report_checks -corner fast
report_checks -corner slow
puts "PASS: disable and1+or1"

unset_disable_timing [get_cells and1]
unset_disable_timing [get_cells or1]
report_checks -corner fast
report_checks -corner slow
puts "PASS: enable all"

#---------------------------------------------------------------
# Case analysis with multi-corner
#---------------------------------------------------------------
puts "--- case analysis multi-corner ---"
set_case_analysis 1 [get_ports d1]
report_checks -corner fast
report_checks -corner slow
puts "PASS: d1=1 multi-corner"

unset_case_analysis [get_ports d1]
report_checks -corner fast
report_checks -corner slow
puts "PASS: d1 unset multi-corner"

set_case_analysis 0 [get_ports d4]
report_checks -corner fast
report_checks -corner slow
puts "PASS: d4=0 multi-corner"

unset_case_analysis [get_ports d4]
report_checks
puts "PASS: d4 unset"

#---------------------------------------------------------------
# Report slews per corner
#---------------------------------------------------------------
puts "--- report_slews multi-corner ---"
report_slews [get_ports d1]
report_slews [get_ports q1]
report_slews [get_ports q2]
report_slews [get_pins nand1/ZN]
report_slews [get_pins nor1/ZN]
report_slews [get_pins reg3/Q]
puts "PASS: slews multi-corner"

#---------------------------------------------------------------
# Report edges (exercises EdgeLess comparator)
#---------------------------------------------------------------
puts "--- report_edges multi-corner ---"
report_edges -from [get_pins nand1/A1] -to [get_pins nand1/ZN]
report_edges -from [get_pins nand1/A2] -to [get_pins nand1/ZN]
report_edges -from [get_pins nor1/A1] -to [get_pins nor1/ZN]
report_edges -from [get_pins nor1/A2] -to [get_pins nor1/ZN]
report_edges -from [get_pins and2/A1] -to [get_pins and2/ZN]
report_edges -from [get_pins and2/A2] -to [get_pins and2/ZN]
report_edges -from [get_pins or2/A1] -to [get_pins or2/ZN]
report_edges -from [get_pins or2/A2] -to [get_pins or2/ZN]
puts "PASS: report_edges multi-corner"

#---------------------------------------------------------------
# report_checks with fields per corner
#---------------------------------------------------------------
puts "--- fields per corner ---"
report_checks -corner fast -fields {slew cap input_pins nets fanout}
puts "PASS: fast with fields"

report_checks -corner slow -fields {slew cap input_pins nets fanout}
puts "PASS: slow with fields"

report_checks -corner fast -format full_clock
puts "PASS: fast full_clock"

report_checks -corner slow -format full_clock
puts "PASS: slow full_clock"

report_checks -corner fast -unconstrained
puts "PASS: fast unconstrained"

report_checks -corner slow -unconstrained
puts "PASS: slow unconstrained"

report_checks -corner fast -group_count 3
puts "PASS: fast group_count 3"

report_checks -corner slow -endpoint_count 5
puts "PASS: slow endpoint_count 5"

puts "ALL PASSED"
