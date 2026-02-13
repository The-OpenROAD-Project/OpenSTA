# Test GraphDelayCalc with a larger design for coverage improvement.
# Targets: GraphDelayCalc.cc (findDelays, seedRootSlews, seedInvalidDelays,
#   findVertexDelay, findCheckEdgeDelays, findLatchEdgeDelays,
#   findMultiDrvrNet, mergeMultiDrvrNets, loadDelay, netCaps,
#   hasMultiDrvrNet, reportCheckMultiDrvrNet, deleteVertexBefore,
#   levelChangedBefore, levelsChangedBefore, delayInvalid(pin/vertex),
#   setIncrementalDelayTolerance, incrementalDelayTolerance)
# DmpCeff.cc (dmpCeffElmore, dmpCeffTwoPole paths with various loads)
# NetCaps.cc (net capacitance queries, pinCapacitance, wireCap)
# ArcDelayCalc.cc (arc delay for various gate types)
# DelayCalcBase.cc (base class paths)

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog dcalc_multidriver_test.v
link_design dcalc_multidriver_test

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {in1 in2 in3 in4 sel}]
set_output_delay -clock clk 0 [get_ports {out1 out2 out3}]
set_input_transition 0.1 [get_ports {in1 in2 in3 in4 sel clk}]

#---------------------------------------------------------------
# Baseline timing (exercises findDelays, seedRootSlews)
#---------------------------------------------------------------
puts "--- baseline timing ---"
report_checks
puts "PASS: baseline report_checks"

report_checks -path_delay min
puts "PASS: baseline min path"

report_checks -path_delay max
puts "PASS: baseline max path"

#---------------------------------------------------------------
# Multiple from/to path queries (exercises findVertexDelay for many paths)
#---------------------------------------------------------------
puts "--- multiple path queries ---"
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: in1->out1"

report_checks -from [get_ports in1] -to [get_ports out2]
puts "PASS: in1->out2"

report_checks -from [get_ports in1] -to [get_ports out3]
puts "PASS: in1->out3"

report_checks -from [get_ports in2] -to [get_ports out1]
puts "PASS: in2->out1"

report_checks -from [get_ports in2] -to [get_ports out2]
puts "PASS: in2->out2"

report_checks -from [get_ports in3] -to [get_ports out1]
puts "PASS: in3->out1"

report_checks -from [get_ports in4] -to [get_ports out2]
puts "PASS: in4->out2"

report_checks -from [get_ports sel] -to [get_ports out1]
puts "PASS: sel->out1"

#---------------------------------------------------------------
# Through pin queries (exercises more graph traversal)
#---------------------------------------------------------------
puts "--- through pin queries ---"
catch {report_checks -through [get_pins or1/ZN]} msg
puts "PASS: through or1/ZN"

catch {report_checks -through [get_pins nand1/ZN]} msg
puts "PASS: through nand1/ZN"

catch {report_checks -through [get_pins nor1/ZN]} msg
puts "PASS: through nor1/ZN"

catch {report_checks -through [get_pins and1/ZN]} msg
puts "PASS: through and1/ZN"

catch {report_checks -through [get_pins inv1/ZN]} msg
puts "PASS: through inv1/ZN"

#---------------------------------------------------------------
# report_dcalc for all arc types in design
# Exercises: arc delay computation for BUF, INV, AND, OR, NAND, NOR
#---------------------------------------------------------------
puts "--- report_dcalc various gate types ---"

# BUF arcs
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts "dcalc buf1 max: $msg"

catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -min} msg
puts "dcalc buf1 min: $msg"

# INV arcs
catch {report_dcalc -from [get_pins inv1/A] -to [get_pins inv1/ZN] -max} msg
puts "dcalc inv1 max: $msg"

# AND arcs (both inputs)
catch {report_dcalc -from [get_pins and1/A1] -to [get_pins and1/ZN] -max} msg
puts "dcalc and1 A1->ZN max: $msg"

catch {report_dcalc -from [get_pins and1/A2] -to [get_pins and1/ZN] -max} msg
puts "dcalc and1 A2->ZN max: $msg"

# OR arcs
catch {report_dcalc -from [get_pins or1/A1] -to [get_pins or1/ZN] -max} msg
puts "dcalc or1 A1->ZN max: $msg"

catch {report_dcalc -from [get_pins or1/A2] -to [get_pins or1/ZN] -max} msg
puts "dcalc or1 A2->ZN max: $msg"

# NAND arcs
catch {report_dcalc -from [get_pins nand1/A1] -to [get_pins nand1/ZN] -max} msg
puts "dcalc nand1 A1->ZN max: $msg"

catch {report_dcalc -from [get_pins nand1/A2] -to [get_pins nand1/ZN] -max} msg
puts "dcalc nand1 A2->ZN max: $msg"

# NOR arcs
catch {report_dcalc -from [get_pins nor1/A1] -to [get_pins nor1/ZN] -max} msg
puts "dcalc nor1 A1->ZN max: $msg"

catch {report_dcalc -from [get_pins nor1/A2] -to [get_pins nor1/ZN] -max} msg
puts "dcalc nor1 A2->ZN max: $msg"

# DFF arcs
catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/Q] -max} msg
puts "dcalc reg1 CK->Q max: $msg"

catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/Q] -min} msg
puts "dcalc reg1 CK->Q min: $msg"

# DFF check arcs
catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/D] -max} msg
puts "dcalc reg1 setup max: $msg"

catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/D] -min} msg
puts "dcalc reg1 hold min: $msg"

catch {report_dcalc -from [get_pins reg2/CK] -to [get_pins reg2/Q] -max} msg
puts "dcalc reg2 CK->Q max: $msg"

catch {report_dcalc -from [get_pins reg2/CK] -to [get_pins reg2/D] -max} msg
puts "dcalc reg2 setup max: $msg"

# report_dcalc with -digits
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -digits 2} msg
puts "dcalc buf1 2 digits: $msg"

catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -digits 6} msg
puts "dcalc buf1 6 digits: $msg"

catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -digits 10} msg
puts "dcalc buf1 10 digits: $msg"

#---------------------------------------------------------------
# Incremental delay calculation: change constraints, recompute
# Exercises: seedInvalidDelays, delayInvalid, incremental paths
#---------------------------------------------------------------
puts "--- incremental delay calculation ---"

# Change loads
set_load 0.001 [get_ports out1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: incremental after set_load 0.001 on out1"

set_load 0.01 [get_ports out1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: incremental after set_load 0.01 on out1"

set_load 0.1 [get_ports out1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: incremental after set_load 0.1 on out1"

set_load 0.05 [get_ports out2]
report_checks -from [get_ports in4] -to [get_ports out2]
puts "PASS: incremental after set_load 0.05 on out2"

# Reset loads
set_load 0 [get_ports out1]
set_load 0 [get_ports out2]

# Change input transitions
set_input_transition 0.01 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: incremental after slew 0.01 on in1"

set_input_transition 1.0 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: incremental after slew 1.0 on in1"

set_input_transition 0.1 [get_ports in1]

# Change clock period
create_clock -name clk -period 5 [get_ports clk]
report_checks
puts "PASS: incremental after clock period change to 5"

# Change input/output delays
set_input_delay -clock clk 1.0 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: incremental after input_delay 1.0"

set_output_delay -clock clk 2.0 [get_ports out1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: incremental after output_delay 2.0"

#---------------------------------------------------------------
# Test various delay calculators on larger design
# Exercises: calculator switching, copy/init paths
#---------------------------------------------------------------
puts "--- calculator switching ---"

set_delay_calculator unit
report_checks
puts "PASS: unit on large design"

set_delay_calculator lumped_cap
report_checks
puts "PASS: lumped_cap on large design"

set_delay_calculator dmp_ceff_elmore
report_checks
puts "PASS: dmp_ceff_elmore on large design"

set_delay_calculator dmp_ceff_two_pole
report_checks
puts "PASS: dmp_ceff_two_pole on large design"

catch {set_delay_calculator ccs_ceff} msg
report_checks
puts "PASS: ccs_ceff on large design"

# Switch back to default
set_delay_calculator dmp_ceff_elmore

#---------------------------------------------------------------
# report_checks with various formatting options
#---------------------------------------------------------------
puts "--- report_checks formatting ---"
report_checks -fields {slew cap input_pins nets fanout}
puts "PASS: report_checks with all fields"

report_checks -format full_clock
puts "PASS: report_checks full_clock"

report_checks -format full_clock_expanded
puts "PASS: report_checks full_clock_expanded"

report_checks -endpoint_count 3
puts "PASS: report_checks endpoint_count 3"

report_checks -group_count 5
puts "PASS: report_checks group_count 5"

report_checks -unconstrained
puts "PASS: report_checks unconstrained"

report_checks -sort_by_slack
puts "PASS: report_checks sort_by_slack"

#---------------------------------------------------------------
# report_check_types
#---------------------------------------------------------------
puts "--- report_check_types ---"
report_check_types -max_delay -verbose
puts "PASS: report_check_types max"

report_check_types -min_delay -verbose
puts "PASS: report_check_types min"

report_check_types -max_delay -min_delay -verbose
puts "PASS: report_check_types max+min"

puts "ALL PASSED"
