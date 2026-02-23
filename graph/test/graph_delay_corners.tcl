# Test graph delay value comparison and multi-corner graph operations.
# Targets: DelayFloat.cc/Graph.cc/GraphCmp.cc multi-corner delay paths.

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Multi-corner setup for graph coverage
#---------------------------------------------------------------
define_corners fast slow

read_liberty -corner fast ../../test/nangate45/Nangate45_fast.lib
read_liberty -corner slow ../../test/nangate45/Nangate45_slow.lib

read_verilog graph_test2.v
link_design graph_test2

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports {d1 d2 en}]
set_output_delay -clock clk 1.0 [get_ports {q1 q2}]
set_input_transition 0.1 [get_ports {d1 d2 en}]

#---------------------------------------------------------------
# Multi-corner timing reports (exercises delay value comparison
# across min/max analysis points in Graph)
#---------------------------------------------------------------
puts "--- fast corner ---"
report_checks -corner fast

report_checks -corner fast -path_delay min

report_checks -corner fast -path_delay max

puts "--- slow corner ---"
report_checks -corner slow

report_checks -corner slow -path_delay min

report_checks -corner slow -path_delay max

#---------------------------------------------------------------
# Multi-corner report_dcalc (exercises delay subtraction/comparison)
#---------------------------------------------------------------
puts "--- report_dcalc per corner ---"
report_dcalc -corner fast -from [get_pins buf1/A] -to [get_pins buf1/Z]
puts "fast buf1 dcalc: done"

report_dcalc -corner slow -from [get_pins buf1/A] -to [get_pins buf1/Z]
puts "slow buf1 dcalc: done"

report_dcalc -corner fast -from [get_pins inv1/A] -to [get_pins inv1/ZN]
puts "fast inv1 dcalc: done"

report_dcalc -corner slow -from [get_pins inv1/A] -to [get_pins inv1/ZN]
puts "slow inv1 dcalc: done"

report_dcalc -corner fast -from [get_pins and1/A1] -to [get_pins and1/ZN]
puts "fast and1 A1 dcalc: done"

report_dcalc -corner slow -from [get_pins and1/A1] -to [get_pins and1/ZN]
puts "slow and1 A1 dcalc: done"

report_dcalc -corner fast -from [get_pins or1/A1] -to [get_pins or1/ZN]
puts "fast or1 A1 dcalc: done"

report_dcalc -corner slow -from [get_pins or1/A1] -to [get_pins or1/ZN]
puts "slow or1 A1 dcalc: done"

# DFF arcs
report_dcalc -corner fast -from [get_pins reg1/CK] -to [get_pins reg1/Q] -max
puts "fast reg1 CK->Q: done"

report_dcalc -corner slow -from [get_pins reg1/CK] -to [get_pins reg1/Q] -max
puts "slow reg1 CK->Q: done"

report_dcalc -corner fast -from [get_pins reg1/CK] -to [get_pins reg1/D] -max
puts "fast reg1 setup: done"

report_dcalc -corner slow -from [get_pins reg1/CK] -to [get_pins reg1/D] -min
puts "slow reg1 hold: done"

#---------------------------------------------------------------
# report_checks with fields across corners (exercises graph slew access)
#---------------------------------------------------------------
puts "--- report_checks with fields ---"
report_checks -corner fast -fields {slew cap input_pins}

report_checks -corner slow -fields {slew cap input_pins}

report_checks -corner fast -format full_clock

report_checks -corner slow -format full_clock

#---------------------------------------------------------------
# Multi-corner paths (different paths in fast vs slow)
#---------------------------------------------------------------
puts "--- multi-corner paths ---"
report_checks -corner fast -from [get_ports d1] -to [get_ports q1]

report_checks -corner slow -from [get_ports d1] -to [get_ports q1]

report_checks -corner fast -from [get_ports d2] -to [get_ports q2]

report_checks -corner slow -from [get_ports d2] -to [get_ports q2]

report_checks -corner fast -from [get_ports en] -to [get_ports q1]

report_checks -corner slow -from [get_ports en] -to [get_ports q1]

#---------------------------------------------------------------
# Edge queries with multi-corner
#---------------------------------------------------------------
puts "--- timing edges multi-corner ---"
set e1 [get_timing_edges -of_objects [get_cells and1]]
puts "and1 edges: [llength $e1]"

set e2 [get_timing_edges -of_objects [get_cells or1]]
puts "or1 edges: [llength $e2]"

set e3 [get_timing_edges -of_objects [get_cells reg1]]
puts "reg1 edges: [llength $e3]"

report_edges -from [get_pins and1/A1] -to [get_pins and1/ZN]
report_edges -from [get_pins and1/A2] -to [get_pins and1/ZN]
report_edges -from [get_pins or1/A1] -to [get_pins or1/ZN]
report_edges -from [get_pins or1/A2] -to [get_pins or1/ZN]

#---------------------------------------------------------------
# Load changes with multi-corner (exercises delay recomputation)
#---------------------------------------------------------------
puts "--- load changes multi-corner ---"
set_load 0.01 [get_ports q1]
set_load 0.05 [get_ports q2]

report_checks -corner fast

report_checks -corner slow

set_load 0 [get_ports q1]
set_load 0 [get_ports q2]

#---------------------------------------------------------------
# report_checks -unconstrained multi-corner
#---------------------------------------------------------------
puts "--- unconstrained multi-corner ---"
report_checks -corner fast -unconstrained

report_checks -corner slow -unconstrained

#---------------------------------------------------------------
# Disable/enable with multi-corner
#---------------------------------------------------------------
puts "--- disable with multi-corner ---"
set_disable_timing [get_cells buf1]
report_checks -corner fast
report_checks -corner slow

unset_disable_timing [get_cells buf1]
report_checks -corner fast
report_checks -corner slow
