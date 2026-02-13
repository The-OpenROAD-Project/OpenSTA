# Test clock sense variants (positive, negative, stop), unset_clock_uncertainty,
# unset_clock_groups, exception priority and overrides.
# Targets:
#   Sdc.cc: setClockSense (with and without clocks, positive/negative/stop),
#     clkStopPropagation, clkStopSense, removeClockSense (via unset_clock_sense),
#     setClockUncertainty, removeClockUncertainty,
#     unsetInterClockUncertainty, deleteInterClockUncertaintiesReferencing,
#     removeClockGroups, clockGroupsAreSame, clockGroupMembers,
#     setClockInsertion/removeClockInsertion,
#     makeGroupPath (multiple from/to merging)
#   WriteSdc.cc: writeClockSenses (positive/negative/stop with clocks),
#     writeClockGroups (logically_exclusive, physically_exclusive, asynchronous),
#     writeClockUncertainty, writeInterClockUncertainties,
#     writeClockInsertions, writePropagatedClkPins,
#     writeExceptions (overriding false_path with multicycle)
#   ExceptionPath.cc: FalsePath::overrides, MultiCyclePath::overrides,
#     PathDelay::overrides, intersectsPts, mergeable,
#     ExceptionFrom::intersectsPts, ExceptionTo::intersectsPts,
#     ExceptionTo with endTransition (rise/fall)
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

############################################################
# Create clocks
############################################################
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
create_clock -name vclk -period 5
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_input_delay -clock clk2 2.0 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]
puts "PASS: setup"

############################################################
# Clock sense: positive, negative, stop
############################################################
# Positive sense on a pin with specific clock
set_clock_sense -positive -clocks [get_clocks clk1] [get_pins buf1/Z]
puts "PASS: clock_sense positive with clock"

# Negative sense on a pin with specific clock
set_clock_sense -negative -clocks [get_clocks clk1] [get_pins inv1/ZN]
puts "PASS: clock_sense negative with clock"

# Stop sense (should prevent clock propagation)
set_clock_sense -stop_propagation -clocks [get_clocks clk2] [get_pins or1/ZN]
puts "PASS: clock_sense stop with clock"

# Clock sense without -clocks (applies to all clocks on pin)
set_clock_sense -positive [get_pins and1/ZN]
puts "PASS: clock_sense positive without clock"

# Clock sense negative without specific clock
set_clock_sense -negative [get_pins nand1/ZN]
puts "PASS: clock_sense negative without clock"

# Write to exercise writeClockSenses
set sdc1 [make_result_file sdc_sense1.sdc]
write_sdc -no_timestamp $sdc1
puts "PASS: write_sdc with clock senses"

# Overwrite clock sense with a different sense on the same pin
# (this exercises the hasKey path in setClockSense)
set_clock_sense -negative -clocks [get_clocks clk1] [get_pins buf1/Z]
puts "PASS: overwrite clock_sense positive with negative"

report_checks
puts "PASS: report after sense changes"

############################################################
# Clock uncertainty: set and unset
############################################################
# Simple uncertainty
set_clock_uncertainty -setup 0.2 [get_clocks clk1]
set_clock_uncertainty -hold 0.1 [get_clocks clk1]
set_clock_uncertainty -setup 0.3 [get_clocks clk2]
set_clock_uncertainty -hold 0.15 [get_clocks clk2]
puts "PASS: clock uncertainty"

# Inter-clock uncertainty
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup 0.35
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -hold 0.18
set_clock_uncertainty -from [get_clocks clk2] -to [get_clocks clk1] -setup 0.32
set_clock_uncertainty -from [get_clocks clk2] -to [get_clocks clk1] -hold 0.16
puts "PASS: inter-clock uncertainty"

# Write before unset
set sdc2 [make_result_file sdc_sense2.sdc]
write_sdc -no_timestamp $sdc2
puts "PASS: write_sdc with uncertainties"

# Unset simple uncertainty
unset_clock_uncertainty -setup [get_clocks clk1]
unset_clock_uncertainty -hold [get_clocks clk1]
puts "PASS: unset clock uncertainty"

# Unset inter-clock uncertainty
unset_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup
unset_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -hold
puts "PASS: unset inter-clock uncertainty"

report_checks
puts "PASS: report after uncertainty unset"

############################################################
# Clock insertion (source latency)
############################################################
set_clock_latency -source -early 0.3 [get_clocks clk1]
set_clock_latency -source -late 0.5 [get_clocks clk1]
set_clock_latency -source -rise -early 0.25 [get_clocks clk1]
set_clock_latency -source -fall -late 0.55 [get_clocks clk1]
puts "PASS: clock insertion"

# Remove clock insertion
unset_clock_latency -source [get_clocks clk1]
puts "PASS: unset clock insertion"

############################################################
# Clock groups: all three types
############################################################
# Asynchronous
set_clock_groups -asynchronous -name async1 -group {clk1} -group {clk2}
puts "PASS: clock_groups asynchronous"

# Unset clock groups
unset_clock_groups -asynchronous -name async1
puts "PASS: unset clock_groups async"

# Logically exclusive
set_clock_groups -logically_exclusive -name logic1 -group {clk1} -group {clk2}
puts "PASS: clock_groups logically_exclusive"

# Unset
unset_clock_groups -logically_exclusive -name logic1
puts "PASS: unset clock_groups logically_exclusive"

# Physically exclusive
set_clock_groups -physically_exclusive -name phys1 -group {clk1} -group {vclk}
puts "PASS: clock_groups physically_exclusive"

# Write to exercise writeClockGroups
set sdc3 [make_result_file sdc_sense3.sdc]
write_sdc -no_timestamp $sdc3
puts "PASS: write_sdc with clock groups"

# Unset
unset_clock_groups -physically_exclusive -name phys1
puts "PASS: unset clock_groups physically_exclusive"

############################################################
# Exception overrides: same from/to with different types
############################################################

# First set a max_delay
set_max_delay -from [get_ports in1] -to [get_ports out1] 8.0
puts "PASS: max_delay"

# Then override with false_path on same from/to
set_false_path -from [get_ports in1] -to [get_ports out1]
puts "PASS: false_path overrides max_delay"

# False path with rise/fall on to endpoint
set_false_path -from [get_ports in2] -rise_to [get_ports out1]
set_false_path -from [get_ports in2] -fall_to [get_ports out1]
puts "PASS: false_path with rise_to and fall_to"

# False path with rise/fall from
set_false_path -rise_from [get_ports in3] -to [get_ports out2]
set_false_path -fall_from [get_ports in3] -to [get_ports out2]
puts "PASS: false_path with rise_from and fall_from"

# Multicycle with setup and hold on same path
set_multicycle_path -setup 2 -from [get_ports in1] -to [get_ports out2]
set_multicycle_path -hold 1 -from [get_ports in1] -to [get_ports out2]
puts "PASS: multicycle setup + hold same path"

# Multicycle between clocks
set_multicycle_path -setup 3 -from [get_clocks clk1] -to [get_clocks clk2]
set_multicycle_path -hold 2 -from [get_clocks clk1] -to [get_clocks clk2]
puts "PASS: multicycle between clocks"

# Max/min delay with through
set_max_delay -from [get_ports in2] -through [get_pins buf1/Z] -to [get_ports out2] 6.0
set_min_delay -from [get_ports in2] -through [get_pins buf1/Z] -to [get_ports out2] 0.5
puts "PASS: max/min delay with through"

# Group path with multiple from objects
group_path -name grp_multi \
  -from [list [get_ports in1] [get_ports in2] [get_clocks clk1]] \
  -to [list [get_ports out1] [get_ports out2]]
puts "PASS: group_path multi from/to"

# Write exceptions with all types
set sdc4 [make_result_file sdc_sense4.sdc]
write_sdc -no_timestamp $sdc4
puts "PASS: write_sdc with all exceptions"

set sdc5 [make_result_file sdc_sense5.sdc]
write_sdc -no_timestamp -compatible $sdc5
puts "PASS: write_sdc compatible"

############################################################
# Unset exceptions and re-report
############################################################
unset_path_exceptions -from [get_ports in1] -to [get_ports out1]
puts "PASS: unset false_path"

unset_path_exceptions -from [get_ports in2] -rise_to [get_ports out1]
unset_path_exceptions -from [get_ports in2] -fall_to [get_ports out1]
puts "PASS: unset rise/fall false_paths"

report_checks
puts "PASS: final report"

############################################################
# Read back SDC
############################################################
read_sdc $sdc4
report_checks
puts "PASS: read_sdc + report"

puts "ALL PASSED"
