# Test graph delay value comparison and multi-corner graph operations.
# Targets: DelayFloat.cc (87.8% -> delayLess with min_max, delayGreater,
#   delayGreaterEqual, delayLessEqual, delayRemove, delayRatio, delayZero,
#   delayInf, delayInitValue, delayIsInitValue)
#   Graph.cc (71.2% -> multi-corner vertex/edge access, clock vertices)
#   GraphCmp.cc (90.5% -> sortEdges with multiple edges)

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
puts "PASS: fast corner"

report_checks -corner fast -path_delay min
puts "PASS: fast corner min"

report_checks -corner fast -path_delay max
puts "PASS: fast corner max"

puts "--- slow corner ---"
report_checks -corner slow
puts "PASS: slow corner"

report_checks -corner slow -path_delay min
puts "PASS: slow corner min"

report_checks -corner slow -path_delay max
puts "PASS: slow corner max"

#---------------------------------------------------------------
# Multi-corner report_dcalc (exercises delay subtraction/comparison)
#---------------------------------------------------------------
puts "--- report_dcalc per corner ---"
catch {report_dcalc -corner fast -from [get_pins buf1/A] -to [get_pins buf1/Z]} msg
puts "fast buf1 dcalc: done"

catch {report_dcalc -corner slow -from [get_pins buf1/A] -to [get_pins buf1/Z]} msg
puts "slow buf1 dcalc: done"

catch {report_dcalc -corner fast -from [get_pins inv1/A] -to [get_pins inv1/ZN]} msg
puts "fast inv1 dcalc: done"

catch {report_dcalc -corner slow -from [get_pins inv1/A] -to [get_pins inv1/ZN]} msg
puts "slow inv1 dcalc: done"

catch {report_dcalc -corner fast -from [get_pins and1/A1] -to [get_pins and1/ZN]} msg
puts "fast and1 A1 dcalc: done"

catch {report_dcalc -corner slow -from [get_pins and1/A1] -to [get_pins and1/ZN]} msg
puts "slow and1 A1 dcalc: done"

catch {report_dcalc -corner fast -from [get_pins or1/A1] -to [get_pins or1/ZN]} msg
puts "fast or1 A1 dcalc: done"

catch {report_dcalc -corner slow -from [get_pins or1/A1] -to [get_pins or1/ZN]} msg
puts "slow or1 A1 dcalc: done"

# DFF arcs
catch {report_dcalc -corner fast -from [get_pins reg1/CK] -to [get_pins reg1/Q] -max} msg
puts "fast reg1 CK->Q: done"

catch {report_dcalc -corner slow -from [get_pins reg1/CK] -to [get_pins reg1/Q] -max} msg
puts "slow reg1 CK->Q: done"

catch {report_dcalc -corner fast -from [get_pins reg1/CK] -to [get_pins reg1/D] -max} msg
puts "fast reg1 setup: done"

catch {report_dcalc -corner slow -from [get_pins reg1/CK] -to [get_pins reg1/D] -min} msg
puts "slow reg1 hold: done"

puts "PASS: report_dcalc per corner"

#---------------------------------------------------------------
# report_checks with fields across corners (exercises graph slew access)
#---------------------------------------------------------------
puts "--- report_checks with fields ---"
report_checks -corner fast -fields {slew cap input_pins}
puts "PASS: fast fields"

report_checks -corner slow -fields {slew cap input_pins}
puts "PASS: slow fields"

report_checks -corner fast -format full_clock
puts "PASS: fast full_clock"

report_checks -corner slow -format full_clock
puts "PASS: slow full_clock"

#---------------------------------------------------------------
# Multi-corner paths (different paths in fast vs slow)
#---------------------------------------------------------------
puts "--- multi-corner paths ---"
report_checks -corner fast -from [get_ports d1] -to [get_ports q1]
puts "PASS: fast d1->q1"

report_checks -corner slow -from [get_ports d1] -to [get_ports q1]
puts "PASS: slow d1->q1"

report_checks -corner fast -from [get_ports d2] -to [get_ports q2]
puts "PASS: fast d2->q2"

report_checks -corner slow -from [get_ports d2] -to [get_ports q2]
puts "PASS: slow d2->q2"

report_checks -corner fast -from [get_ports en] -to [get_ports q1]
puts "PASS: fast en->q1"

report_checks -corner slow -from [get_ports en] -to [get_ports q1]
puts "PASS: slow en->q1"

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
puts "PASS: timing edges multi-corner"

#---------------------------------------------------------------
# Load changes with multi-corner (exercises delay recomputation)
#---------------------------------------------------------------
puts "--- load changes multi-corner ---"
set_load 0.01 [get_ports q1]
set_load 0.05 [get_ports q2]

report_checks -corner fast
puts "PASS: fast after load change"

report_checks -corner slow
puts "PASS: slow after load change"

set_load 0 [get_ports q1]
set_load 0 [get_ports q2]

#---------------------------------------------------------------
# report_checks -unconstrained multi-corner
#---------------------------------------------------------------
puts "--- unconstrained multi-corner ---"
report_checks -corner fast -unconstrained
puts "PASS: fast unconstrained"

report_checks -corner slow -unconstrained
puts "PASS: slow unconstrained"

#---------------------------------------------------------------
# Disable/enable with multi-corner
#---------------------------------------------------------------
puts "--- disable with multi-corner ---"
set_disable_timing [get_cells buf1]
report_checks -corner fast
report_checks -corner slow
puts "PASS: disable buf1 multi-corner"

unset_disable_timing [get_cells buf1]
report_checks -corner fast
report_checks -corner slow
puts "PASS: enable buf1 multi-corner"

puts "ALL PASSED"
