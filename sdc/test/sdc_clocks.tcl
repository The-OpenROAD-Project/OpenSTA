# Test comprehensive clock SDC commands for code coverage
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

############################################################
# create_clock with various options
############################################################

# Basic clock
create_clock -name clk1 -period 10 [get_ports clk1]

# Clock with waveform
create_clock -name clk2 -period 20 -waveform {0 10} [get_ports clk2]

# Virtual clock (no port)
create_clock -name vclk -period 5

# Clock with -add on same port (additional clock definition)
create_clock -name clk1_fast -period 5 -add [get_ports clk1]

# Report clock properties
report_clock_properties

############################################################
# create_generated_clock
############################################################

create_generated_clock -name gen_clk_div2 -source [get_ports clk1] -divide_by 2 [get_pins reg1/Q]

create_generated_clock -name gen_clk_mul3 -source [get_ports clk2] -multiply_by 3 [get_pins reg3/Q]

report_clock_properties

############################################################
# set_clock_latency
############################################################

# Source latency with rise/fall min/max
set_clock_latency -source 0.5 [get_clocks clk1]

set_clock_latency -source -rise -max 0.6 [get_clocks clk1]

set_clock_latency -source -fall -min 0.3 [get_clocks clk1]

# Network latency
set_clock_latency 0.2 [get_clocks clk2]

set_clock_latency -rise -max 0.4 [get_clocks clk2]

set_clock_latency -fall -min 0.1 [get_clocks clk2]

############################################################
# set_clock_uncertainty
############################################################

# Simple clock uncertainty
set_clock_uncertainty -setup 0.2 [get_clocks clk1]

set_clock_uncertainty -hold 0.1 [get_clocks clk1]

# Inter-clock uncertainty
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup 0.3

set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -hold 0.15

############################################################
# set_clock_transition
############################################################

set_clock_transition -rise -max 0.15 [get_clocks clk1]

set_clock_transition -fall -min 0.08 [get_clocks clk1]

set_clock_transition 0.1 [get_clocks clk2]

############################################################
# set_propagated_clock
############################################################

set_propagated_clock [get_clocks clk1]

############################################################
# set_clock_groups
############################################################

set_clock_groups -logically_exclusive -group [get_clocks clk1] -group [get_clocks clk2]

# Remove and re-add as physically exclusive
unset_clock_groups -logically_exclusive -all

set_clock_groups -physically_exclusive -group [get_clocks clk1] -group [get_clocks clk2]

unset_clock_groups -physically_exclusive -all

set_clock_groups -asynchronous -group [get_clocks clk1] -group [get_clocks clk2]

unset_clock_groups -asynchronous -all

############################################################
# set_clock_sense
############################################################

set_clock_sense -positive -clocks [get_clocks clk1] [get_pins buf1/Z]

############################################################
# report_checks to verify constraint effects
############################################################

report_checks -from [get_ports in1] -to [get_ports out1]

############################################################
# Unset/cleanup operations
############################################################

unset_propagated_clock [get_clocks clk1]

unset_clock_latency [get_clocks clk1]

unset_clock_latency [get_clocks clk2]

unset_clock_latency -source [get_clocks clk1]

unset_clock_transition [get_clocks clk1]

unset_clock_uncertainty -setup [get_clocks clk1]

unset_clock_uncertainty -hold [get_clocks clk1]

unset_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup

unset_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -hold

# Delete generated clocks
delete_generated_clock [get_clocks gen_clk_div2]

delete_generated_clock [get_clocks gen_clk_mul3]

# Delete regular clocks
delete_clock [get_clocks vclk]

report_clock_properties
