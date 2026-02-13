# Test clock groups and clock sense for code coverage
# Targets: Sdc.cc (makeClockGroups, makeClkGroupExclusions, sameClockGroup,
#          setClockSense, clkStopPropagation, removeClockGroups*),
#          ClockGroups.cc, WriteSdc.cc (writeClockGroups, writeClockSense),
#          Clock.cc (edge queries, waveform handling)
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

############################################################
# Create multiple clocks
############################################################

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 -waveform {0 10} [get_ports clk2]
create_clock -name vclk1 -period 8
create_clock -name vclk2 -period 12
create_clock -name clk1_2x -period 5 -add [get_ports clk1]
puts "PASS: clocks created"

# Generated clocks
create_generated_clock -name gclk1 -source [get_ports clk1] -divide_by 2 [get_pins reg1/Q]
create_generated_clock -name gclk2 -source [get_ports clk2] -multiply_by 2 [get_pins reg3/Q]
puts "PASS: generated clocks"

# IO delays
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_input_delay -clock clk2 2.0 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]
puts "PASS: IO delays"

############################################################
# Clock groups - asynchronous
############################################################

set_clock_groups -asynchronous -name async_group1 \
  -group {clk1 clk1_2x gclk1} \
  -group {clk2 gclk2}
puts "PASS: clock_groups -asynchronous"

# Write SDC with async groups
set sdc_file1 [make_result_file sdc_clk_grp_async.sdc]
write_sdc -no_timestamp $sdc_file1
puts "PASS: write_sdc with async groups"

report_checks
puts "PASS: report_checks after async groups"

# Remove async groups
unset_clock_groups -asynchronous -name async_group1
puts "PASS: unset_clock_groups async"

############################################################
# Clock groups - logically exclusive
############################################################

set_clock_groups -logically_exclusive -name logical_group1 \
  -group {clk1 clk1_2x} \
  -group {vclk1}
puts "PASS: clock_groups -logically_exclusive"

# Write SDC with logically exclusive groups
set sdc_file2 [make_result_file sdc_clk_grp_logical.sdc]
write_sdc -no_timestamp $sdc_file2
puts "PASS: write_sdc with logical groups"

# Remove logically exclusive
unset_clock_groups -logically_exclusive -name logical_group1
puts "PASS: unset logically_exclusive"

############################################################
# Clock groups - physically exclusive
############################################################

set_clock_groups -physically_exclusive -name phys_group1 \
  -group {clk1} \
  -group {clk1_2x}
puts "PASS: clock_groups -physically_exclusive"

# Write SDC
set sdc_file3 [make_result_file sdc_clk_grp_phys.sdc]
write_sdc -no_timestamp $sdc_file3
puts "PASS: write_sdc with physical groups"

# Remove physically exclusive
unset_clock_groups -physically_exclusive -name phys_group1
puts "PASS: unset physically_exclusive"

############################################################
# Multiple clock groups simultaneously
############################################################

set_clock_groups -asynchronous -name mixed1 \
  -group {clk1 gclk1} \
  -group {clk2 gclk2}

set_clock_groups -logically_exclusive -name mixed2 \
  -group {vclk1} \
  -group {vclk2}

puts "PASS: multiple clock groups simultaneously"

# Write SDC with multiple groups
set sdc_file4 [make_result_file sdc_clk_grp_multi.sdc]
write_sdc -no_timestamp $sdc_file4
puts "PASS: write_sdc with multiple groups"

report_checks
puts "PASS: report_checks after multiple groups"

# Remove all
unset_clock_groups -asynchronous -name mixed1
unset_clock_groups -logically_exclusive -name mixed2
puts "PASS: unset multiple groups"

############################################################
# Clock groups using -allow_paths
############################################################

set_clock_groups -asynchronous -name allow_grp \
  -group {clk1 clk1_2x gclk1} \
  -group {clk2 gclk2} \
  -allow_paths

puts "PASS: clock_groups -allow_paths"

set sdc_file5 [make_result_file sdc_clk_grp_allow.sdc]
write_sdc -no_timestamp $sdc_file5
puts "PASS: write_sdc with -allow_paths"

unset_clock_groups -asynchronous -name allow_grp
puts "PASS: unset -allow_paths groups"

############################################################
# Clock sense
############################################################

# Positive sense
set_clock_sense -positive -clocks [get_clocks clk1] [get_pins buf1/Z]
puts "PASS: clock_sense -positive"

# Negative sense
set_clock_sense -negative -clocks [get_clocks clk2] [get_pins inv1/ZN]
puts "PASS: clock_sense -negative"

# Stop propagation
set_clock_sense -stop_propagation -clocks [get_clocks clk1] [get_pins and1/ZN]
puts "PASS: clock_sense -stop_propagation"

# Write SDC with clock sense
set sdc_file6 [make_result_file sdc_clk_sense.sdc]
write_sdc -no_timestamp $sdc_file6
puts "PASS: write_sdc with clock sense"

report_checks
puts "PASS: report_checks after clock sense"

############################################################
# Clock groups without explicit name (auto-naming)
############################################################

set_clock_groups -asynchronous \
  -group {clk1 gclk1} \
  -group {clk2 gclk2}
puts "PASS: clock_groups without name (auto-named)"

unset_clock_groups -asynchronous -all
puts "PASS: unset_clock_groups -all"

############################################################
# Propagated clock on pins
############################################################

set_propagated_clock [get_clocks clk1]
set_propagated_clock [get_clocks clk2]
set_propagated_clock [get_pins reg1/CK]
puts "PASS: set_propagated_clock"

unset_propagated_clock [get_clocks clk1]
unset_propagated_clock [get_pins reg1/CK]
puts "PASS: unset_propagated_clock"

############################################################
# Clock uncertainty on pins
############################################################

set_clock_uncertainty -setup 0.15 [get_pins reg1/CK]
set_clock_uncertainty -hold 0.08 [get_pins reg1/CK]
puts "PASS: clock_uncertainty on pin"

set_clock_uncertainty -setup 0.2 [get_pins reg2/CK]
puts "PASS: clock_uncertainty on reg2/CK"

# Inter-clock uncertainty
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup 0.3
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -hold 0.15
set_clock_uncertainty -from [get_clocks clk2] -to [get_clocks clk1] -setup 0.25
set_clock_uncertainty -from [get_clocks clk2] -to [get_clocks clk1] -hold 0.12
puts "PASS: inter-clock uncertainty"

# Write SDC with all uncertainty
set sdc_file7 [make_result_file sdc_clk_uncert.sdc]
write_sdc -no_timestamp $sdc_file7
puts "PASS: write_sdc with uncertainty"

# Remove inter-clock uncertainty
unset_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup
unset_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -hold
puts "PASS: unset inter-clock uncertainty"

# Remove pin uncertainty
unset_clock_uncertainty -setup [get_pins reg1/CK]
unset_clock_uncertainty -hold [get_pins reg1/CK]
puts "PASS: unset pin uncertainty"

############################################################
# Final write/verify
############################################################

set sdc_final [make_result_file sdc_clk_grp_final.sdc]
write_sdc -no_timestamp $sdc_final
puts "PASS: final write_sdc"

report_checks
puts "PASS: final report_checks"

puts "ALL PASSED"
