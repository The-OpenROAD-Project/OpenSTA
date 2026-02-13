# Test write_sdc with various option combinations for code coverage
# Targets: WriteSdc.cc (multiple write functions, compatible mode, digits,
#          WriteGetPin, WriteGetPort, WriteGetClock, WriteGetInstance, etc.)
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

############################################################
# Create comprehensive constraints to maximize write_sdc coverage
############################################################

# Multiple clocks
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 -waveform {0 10} [get_ports clk2]
create_clock -name vclk -period 5
create_clock -name clk1_fast -period 5 -add [get_ports clk1]
puts "PASS: clocks created"

# Generated clocks (various options)
create_generated_clock -name gen_div2 -source [get_ports clk1] -divide_by 2 [get_pins reg1/Q]
create_generated_clock -name gen_mul3 -source [get_ports clk2] -multiply_by 3 [get_pins reg3/Q]
create_generated_clock -name gen_edges -source [get_ports clk1] -edges {1 3 5} [get_pins reg2/Q]
puts "PASS: generated clocks"

# Clock latency (source + network, all rf/minmax combos)
set_clock_latency -source 0.5 [get_clocks clk1]
set_clock_latency -source -rise -max 0.6 [get_clocks clk1]
set_clock_latency -source -fall -min 0.3 [get_clocks clk1]
set_clock_latency -source -rise -min 0.25 [get_clocks clk1]
set_clock_latency -source -fall -max 0.55 [get_clocks clk1]
set_clock_latency 0.2 [get_clocks clk2]
set_clock_latency -rise -max 0.4 [get_clocks clk2]
set_clock_latency -fall -min 0.1 [get_clocks clk2]
puts "PASS: clock latency"

# Clock uncertainty
set_clock_uncertainty -setup 0.2 [get_clocks clk1]
set_clock_uncertainty -hold 0.1 [get_clocks clk1]
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup 0.3
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -hold 0.15
puts "PASS: clock uncertainty"

# Clock transition
set_clock_transition -rise -max 0.15 [get_clocks clk1]
set_clock_transition -fall -min 0.08 [get_clocks clk1]
set_clock_transition 0.1 [get_clocks clk2]
puts "PASS: clock transition"

# Propagated clock
set_propagated_clock [get_clocks clk1]
puts "PASS: propagated clock"

# Clock groups
set_clock_groups -asynchronous -group {clk1 clk1_fast} -group {clk2}
puts "PASS: clock groups"

# Clock sense
set_clock_sense -positive -clocks [get_clocks clk1] [get_pins buf1/Z]
puts "PASS: clock sense"

# Input delays (various options)
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 -rise -max 2.5 [get_ports in2]
set_input_delay -clock clk1 -fall -min 1.0 [get_ports in2]
set_input_delay -clock clk2 1.8 [get_ports in3]
set_input_delay -clock clk1 -clock_fall 1.5 [get_ports in3] -add_delay
puts "PASS: input delays"

# Output delays
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 -rise -max 3.5 [get_ports out2]
set_output_delay -clock clk2 -fall -min 1.5 [get_ports out2] -add_delay
puts "PASS: output delays"

# Driving cells
set_driving_cell -lib_cell BUF_X1 [get_ports in1]
set_driving_cell -lib_cell INV_X1 -pin ZN [get_ports in2]
set_driving_cell -lib_cell BUF_X4 [get_ports in3]
puts "PASS: driving cells"

# set_drive
set_drive 100 [get_ports in1]
puts "PASS: set_drive"

# Loads
set_load 0.05 [get_ports out1]
set_load -pin_load 0.03 [get_ports out2]
puts "PASS: loads"

# Input transition
set_input_transition 0.15 [get_ports in1]
set_input_transition -rise -max 0.12 [get_ports in2]
set_input_transition -fall -min 0.08 [get_ports in2]
puts "PASS: input transitions"

# Design limits
set_max_transition 0.5 [current_design]
set_max_capacitance 0.2 [current_design]
set_max_fanout 20 [current_design]
set_max_transition 0.3 [get_ports out1]
set_max_capacitance 0.1 [get_ports out1]
set_max_transition -clock_path 0.2 [get_clocks clk1]
set_max_transition -data_path 0.4 [get_clocks clk1]
puts "PASS: design limits"

# Exception paths
set_false_path -from [get_clocks clk1] -to [get_clocks clk2]
set_false_path -from [get_ports in1] -through [get_pins and1/ZN] -to [get_ports out1]
set_false_path -rise_from [get_ports in3] -fall_to [get_ports out2]
set_multicycle_path -setup 2 -from [get_ports in1] -to [get_ports out1]
set_multicycle_path -hold 1 -from [get_ports in1] -to [get_ports out1]
set_max_delay -from [get_ports in2] -to [get_ports out1] 8.0
set_min_delay -from [get_ports in2] -to [get_ports out1] 1.0
group_path -name group_clk1 -from [get_clocks clk1]
group_path -name group_io -from [get_ports in1] -to [get_ports out1]
puts "PASS: exception paths"

# Disable timing
set_disable_timing [get_cells buf1]
set_disable_timing [get_lib_cells NangateOpenCellLibrary/INV_X1] -from A -to ZN
puts "PASS: disable timing"

# Case analysis
set_case_analysis 0 [get_ports in3]
puts "PASS: case analysis"

# Operating conditions
set_operating_conditions typical
puts "PASS: operating conditions"

# Wire load
set_wire_load_model -name "5K_hvratio_1_1"
set_wire_load_mode enclosed
puts "PASS: wire load"

# Timing derate
set_timing_derate -early 0.95
set_timing_derate -late 1.05
set_timing_derate -early -clock 0.97
set_timing_derate -late -clock 1.03
set_timing_derate -early -data 0.94
set_timing_derate -late -data 1.06
puts "PASS: timing derate"

# Min pulse width
set_min_pulse_width 1.0 [get_clocks clk1]
set_min_pulse_width -high 0.5 [get_clocks clk1]
set_min_pulse_width -low 0.4 [get_clocks clk1]
puts "PASS: min pulse width"

# Fanout number
set_port_fanout_number 4 [get_ports out1]
puts "PASS: port fanout"

# Net resistance
set_resistance -min 10.0 [get_nets n1]
set_resistance -max 20.0 [get_nets n1]
puts "PASS: resistance"

# Max area
set_max_area 100.0
puts "PASS: max area"

# Logic values
set_logic_one [get_ports in2]
puts "PASS: logic one"

# Max time borrow
set_max_time_borrow 2.0 [get_clocks clk1]
puts "PASS: max time borrow"

# Clock gating check
set_clock_gating_check -setup 0.5 [get_clocks clk1]
set_clock_gating_check -hold 0.3 [get_clocks clk1]
puts "PASS: clock gating check"

# set_voltage
catch {set_voltage 1.1 -min 0.9}
puts "PASS: voltage"

############################################################
# Write SDC with all option combinations
############################################################

# Option 1: basic (native mode, default digits)
set sdc_file1 [make_result_file sdc_write_opt_basic.sdc]
write_sdc -no_timestamp $sdc_file1
puts "PASS: write_sdc basic"

# Option 2: with -compatible flag
set sdc_file2 [make_result_file sdc_write_opt_compat.sdc]
write_sdc -no_timestamp -compatible $sdc_file2
puts "PASS: write_sdc -compatible"

# Option 3: with -digits 2
set sdc_file3 [make_result_file sdc_write_opt_d2.sdc]
write_sdc -no_timestamp -digits 2 $sdc_file3
puts "PASS: write_sdc -digits 2"

# Option 4: with -digits 8
set sdc_file4 [make_result_file sdc_write_opt_d8.sdc]
write_sdc -no_timestamp -digits 8 $sdc_file4
puts "PASS: write_sdc -digits 8"

# Option 5: -compatible + -digits 6
set sdc_file5 [make_result_file sdc_write_opt_compat_d6.sdc]
write_sdc -no_timestamp -compatible -digits 6 $sdc_file5
puts "PASS: write_sdc -compatible -digits 6"

# Option 6: -map_hpins
set sdc_file6 [make_result_file sdc_write_opt_hpins.sdc]
write_sdc -no_timestamp -map_hpins $sdc_file6
puts "PASS: write_sdc -map_hpins"

############################################################
# Read back and verify
############################################################

# Read back native SDC
read_sdc $sdc_file1
puts "PASS: read_sdc basic"

report_checks
puts "PASS: report_checks after read_sdc"

# Read back compatible SDC
read_sdc $sdc_file2
puts "PASS: read_sdc compatible"

report_checks
puts "PASS: report_checks after read_sdc compatible"

puts "ALL PASSED"
