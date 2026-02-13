# Test comprehensive clock SDC commands for code coverage
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

############################################################
# create_clock with various options
############################################################

# Basic clock
create_clock -name clk1 -period 10 [get_ports clk1]
puts "PASS: create_clock clk1"

# Clock with waveform
create_clock -name clk2 -period 20 -waveform {0 10} [get_ports clk2]
puts "PASS: create_clock clk2 with waveform"

# Virtual clock (no port)
create_clock -name vclk -period 5
puts "PASS: create_clock virtual clock"

# Clock with -add on same port (additional clock definition)
create_clock -name clk1_fast -period 5 -add [get_ports clk1]
puts "PASS: create_clock -add"

# Report clock properties
report_clock_properties
puts "PASS: report_clock_properties"

############################################################
# create_generated_clock
############################################################

create_generated_clock -name gen_clk_div2 -source [get_ports clk1] -divide_by 2 [get_pins reg1/Q]
puts "PASS: create_generated_clock -divide_by"

create_generated_clock -name gen_clk_mul3 -source [get_ports clk2] -multiply_by 3 [get_pins reg3/Q]
puts "PASS: create_generated_clock -multiply_by"

report_clock_properties
puts "PASS: report_clock_properties after generated clocks"

############################################################
# set_clock_latency
############################################################

# Source latency with rise/fall min/max
set_clock_latency -source 0.5 [get_clocks clk1]
puts "PASS: set_clock_latency -source"

set_clock_latency -source -rise -max 0.6 [get_clocks clk1]
puts "PASS: set_clock_latency -source -rise -max"

set_clock_latency -source -fall -min 0.3 [get_clocks clk1]
puts "PASS: set_clock_latency -source -fall -min"

# Network latency
set_clock_latency 0.2 [get_clocks clk2]
puts "PASS: set_clock_latency network"

set_clock_latency -rise -max 0.4 [get_clocks clk2]
puts "PASS: set_clock_latency -rise -max"

set_clock_latency -fall -min 0.1 [get_clocks clk2]
puts "PASS: set_clock_latency -fall -min"

############################################################
# set_clock_uncertainty
############################################################

# Simple clock uncertainty
set_clock_uncertainty -setup 0.2 [get_clocks clk1]
puts "PASS: set_clock_uncertainty -setup"

set_clock_uncertainty -hold 0.1 [get_clocks clk1]
puts "PASS: set_clock_uncertainty -hold"

# Inter-clock uncertainty
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup 0.3
puts "PASS: set_clock_uncertainty -from -to -setup"

set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -hold 0.15
puts "PASS: set_clock_uncertainty -from -to -hold"

############################################################
# set_clock_transition
############################################################

set_clock_transition -rise -max 0.15 [get_clocks clk1]
puts "PASS: set_clock_transition -rise -max"

set_clock_transition -fall -min 0.08 [get_clocks clk1]
puts "PASS: set_clock_transition -fall -min"

set_clock_transition 0.1 [get_clocks clk2]
puts "PASS: set_clock_transition"

############################################################
# set_propagated_clock
############################################################

set_propagated_clock [get_clocks clk1]
puts "PASS: set_propagated_clock"

############################################################
# set_clock_groups
############################################################

set_clock_groups -logically_exclusive -group [get_clocks clk1] -group [get_clocks clk2]
puts "PASS: set_clock_groups -logically_exclusive"

# Remove and re-add as physically exclusive
unset_clock_groups -logically_exclusive -all
puts "PASS: unset_clock_groups -logically_exclusive"

set_clock_groups -physically_exclusive -group [get_clocks clk1] -group [get_clocks clk2]
puts "PASS: set_clock_groups -physically_exclusive"

unset_clock_groups -physically_exclusive -all
puts "PASS: unset_clock_groups -physically_exclusive"

set_clock_groups -asynchronous -group [get_clocks clk1] -group [get_clocks clk2]
puts "PASS: set_clock_groups -asynchronous"

unset_clock_groups -asynchronous -all
puts "PASS: unset_clock_groups -asynchronous"

############################################################
# set_clock_sense
############################################################

set_clock_sense -positive -clocks [get_clocks clk1] [get_pins buf1/Z]
puts "PASS: set_clock_sense -positive"

############################################################
# report_checks to verify constraint effects
############################################################

report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: report_checks after clock constraints"

############################################################
# Unset/cleanup operations
############################################################

unset_propagated_clock [get_clocks clk1]
puts "PASS: unset_propagated_clock"

unset_clock_latency [get_clocks clk1]
puts "PASS: unset_clock_latency"

unset_clock_latency [get_clocks clk2]
puts "PASS: unset_clock_latency clk2"

unset_clock_latency -source [get_clocks clk1]
puts "PASS: unset_clock_latency -source"

unset_clock_transition [get_clocks clk1]
puts "PASS: unset_clock_transition"

unset_clock_uncertainty -setup [get_clocks clk1]
puts "PASS: unset_clock_uncertainty -setup"

unset_clock_uncertainty -hold [get_clocks clk1]
puts "PASS: unset_clock_uncertainty -hold"

unset_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup
puts "PASS: unset_clock_uncertainty -from -to -setup"

unset_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -hold
puts "PASS: unset_clock_uncertainty -from -to -hold"

# Delete generated clocks
delete_generated_clock [get_clocks gen_clk_div2]
puts "PASS: delete_generated_clock gen_clk_div2"

delete_generated_clock [get_clocks gen_clk_mul3]
puts "PASS: delete_generated_clock gen_clk_mul3"

# Delete regular clocks
delete_clock [get_clocks vclk]
puts "PASS: delete_clock vclk"

report_clock_properties
puts "PASS: final report_clock_properties"

puts "ALL PASSED"
