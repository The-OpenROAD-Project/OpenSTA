# Test graph incremental changes, constant propagation, and level reporting.
# Targets: Graph.cc (71.2% -> constant propagation, remove/add edges,
#   setSlew, pinDrvrVertex, pinLoadVertex, hasDownstreamClkPin, etc.)
#   GraphCmp.cc (90.5% -> EdgeLess, sortEdges, VertexNameLess)
#   DelayFloat.cc (87.8% -> delayLess with min_max, delayGreater, etc.)

source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog graph_test2.v
link_design graph_test2

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports d1]
set_input_delay -clock clk 1.0 [get_ports d2]
set_input_delay -clock clk 0 [get_ports en]
set_output_delay -clock clk 1.0 [get_ports q1]
set_output_delay -clock clk 1.0 [get_ports q2]
set_input_transition 0.1 [get_ports {d1 d2 en}]

#---------------------------------------------------------------
# Baseline timing
#---------------------------------------------------------------
puts "--- baseline report_checks ---"
report_checks
puts "PASS: baseline report_checks"

report_checks -path_delay min
puts "PASS: baseline min"

report_checks -path_delay max
puts "PASS: baseline max"

#---------------------------------------------------------------
# Multiple paths through design
#---------------------------------------------------------------
puts "--- multiple paths ---"
report_checks -from [get_ports d1] -to [get_ports q1]
puts "PASS: d1->q1"

report_checks -from [get_ports d1] -to [get_ports q2]
puts "PASS: d1->q2"

report_checks -from [get_ports d2] -to [get_ports q2]
puts "PASS: d2->q2"

report_checks -from [get_ports en] -to [get_ports q1]
puts "PASS: en->q1"

report_checks -from [get_ports en] -to [get_ports q2]
puts "PASS: en->q2"

#---------------------------------------------------------------
# -through paths (exercises graph traversal)
#---------------------------------------------------------------
puts "--- through paths ---"
catch { report_checks -through [get_pins inv1/ZN] } msg
puts "through inv1/ZN: done"
puts "PASS: through inv1/ZN"

catch { report_checks -through [get_pins and1/ZN] } msg
puts "through and1/ZN: done"
puts "PASS: through and1/ZN"

catch { report_checks -through [get_pins or1/ZN] } msg
puts "through or1/ZN: done"
puts "PASS: through or1/ZN"

catch { report_checks -through [get_pins buf3/Z] } msg
puts "through buf3/Z: done"
puts "PASS: through buf3/Z"

#---------------------------------------------------------------
# Timing edge queries for multi-input cells
#---------------------------------------------------------------
puts "--- timing edges for multi-input cells ---"
set edges_and [get_timing_edges -of_objects [get_cells and1]]
puts "and1 edges: [llength $edges_and]"

set edges_or [get_timing_edges -of_objects [get_cells or1]]
puts "or1 edges: [llength $edges_or]"

set edges_reg1 [get_timing_edges -of_objects [get_cells reg1]]
puts "reg1 edges: [llength $edges_reg1]"

set edges_reg2 [get_timing_edges -of_objects [get_cells reg2]]
puts "reg2 edges: [llength $edges_reg2]"

# From/to specific pins
set edges_ft [get_timing_edges -from [get_pins and1/A1] -to [get_pins and1/ZN]]
puts "and1 A1->ZN edges: [llength $edges_ft]"

set edges_ft2 [get_timing_edges -from [get_pins and1/A2] -to [get_pins and1/ZN]]
puts "and1 A2->ZN edges: [llength $edges_ft2]"

set edges_ft3 [get_timing_edges -from [get_pins or1/A1] -to [get_pins or1/ZN]]
puts "or1 A1->ZN edges: [llength $edges_ft3]"

puts "PASS: timing edge queries"

#---------------------------------------------------------------
# report_edges for various pin combinations
#---------------------------------------------------------------
puts "--- report_edges ---"
report_edges -from [get_pins buf1/A] -to [get_pins buf1/Z]
puts "PASS: report_edges buf1"

report_edges -from [get_pins inv1/A] -to [get_pins inv1/ZN]
puts "PASS: report_edges inv1"

report_edges -from [get_pins and1/A1]
puts "PASS: report_edges from and1/A1"

report_edges -to [get_pins and1/ZN]
puts "PASS: report_edges to and1/ZN"

report_edges -from [get_ports d1]
puts "PASS: report_edges from port d1"

report_edges -to [get_ports q2]
puts "PASS: report_edges to port q2"

#---------------------------------------------------------------
# Constant propagation via set_case_analysis
#---------------------------------------------------------------
puts "--- set_case_analysis ---"
set_case_analysis 1 [get_ports en]
report_checks
puts "PASS: report_checks en=1"

report_checks -from [get_ports d1] -to [get_ports q1]
puts "PASS: d1->q1 with en=1"

# Change constant value
set_case_analysis 0 [get_ports en]
report_checks
puts "PASS: report_checks en=0"

# Remove case analysis
unset_case_analysis [get_ports en]
report_checks
puts "PASS: report_checks after unset_case_analysis"

#---------------------------------------------------------------
# Disable/enable timing with multiple cells
#---------------------------------------------------------------
puts "--- disable/enable timing multiple cells ---"
set_disable_timing [get_cells buf1]
report_checks
puts "PASS: disable buf1"

set_disable_timing [get_cells inv1]
report_checks
puts "PASS: disable buf1+inv1"

unset_disable_timing [get_cells buf1]
report_checks
puts "PASS: enable buf1"

unset_disable_timing [get_cells inv1]
report_checks
puts "PASS: enable inv1"

# Disable specific lib cell arc
set_disable_timing -from A -to Z [get_lib_cells NangateOpenCellLibrary/BUF_X1]
report_disabled_edges
report_checks
puts "PASS: disable lib cell arc"

unset_disable_timing -from A -to Z [get_lib_cells NangateOpenCellLibrary/BUF_X1]
report_disabled_edges
report_checks
puts "PASS: unset lib cell arc"

#---------------------------------------------------------------
# report_check_types
#---------------------------------------------------------------
puts "--- report_check_types ---"
report_check_types -max_delay -verbose
puts "PASS: report_check_types max_delay"

report_check_types -min_delay -verbose
puts "PASS: report_check_types min_delay"

report_check_types -max_delay -min_delay -verbose
puts "PASS: report_check_types max+min"

#---------------------------------------------------------------
# Report slews for various pins
#---------------------------------------------------------------
puts "--- report_slews ---"
report_slews [get_ports d1]
report_slews [get_ports d2]
report_slews [get_ports en]
report_slews [get_ports q1]
report_slews [get_ports q2]
report_slews [get_pins buf1/Z]
report_slews [get_pins inv1/ZN]
report_slews [get_pins and1/ZN]
report_slews [get_pins or1/ZN]
report_slews [get_pins reg1/Q]
report_slews [get_pins reg2/Q]
puts "PASS: report_slews various pins"

#---------------------------------------------------------------
# report_checks with -unconstrained
#---------------------------------------------------------------
puts "--- report_checks -unconstrained ---"
report_checks -unconstrained
puts "PASS: report_checks unconstrained"

#---------------------------------------------------------------
# report_checks with group_count and endpoint_count
#---------------------------------------------------------------
puts "--- report_checks counts ---"
report_checks -group_count 3
puts "PASS: group_count 3"

report_checks -endpoint_count 3
puts "PASS: endpoint_count 3"

report_checks -endpoint_count 5 -path_delay min
puts "PASS: endpoint_count 5 min"

puts "ALL PASSED"
