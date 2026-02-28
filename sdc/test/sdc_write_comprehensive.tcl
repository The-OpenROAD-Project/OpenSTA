# Test comprehensive write_sdc for WriteSdc.cc code coverage
# Also exercises many Sdc.cc, Clock.cc, ExceptionPath.cc paths
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

############################################################
# Create multiple clocks with various options
############################################################

# Basic clocks
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 -waveform {0 10} [get_ports clk2]
create_clock -name vclk -period 5
create_clock -name clk1_fast -period 5 -add [get_ports clk1]

# Generated clock
create_generated_clock -name gen_div2 -source [get_ports clk1] -divide_by 2 [get_pins reg1/Q]
create_generated_clock -name gen_mul3 -source [get_ports clk2] -multiply_by 3 [get_pins reg3/Q]

# Propagated clock
set_propagated_clock [get_clocks clk1]

############################################################
# Clock constraints
############################################################

# Clock latency (source and network)
set_clock_latency -source 0.5 [get_clocks clk1]
set_clock_latency -source -rise -max 0.6 [get_clocks clk1]
set_clock_latency -source -fall -min 0.3 [get_clocks clk1]
set_clock_latency 0.2 [get_clocks clk2]
set_clock_latency -rise -max 0.4 [get_clocks clk2]
set_clock_latency -fall -min 0.1 [get_clocks clk2]

# Clock uncertainty (simple and inter-clock)
set_clock_uncertainty -setup 0.2 [get_clocks clk1]
set_clock_uncertainty -hold 0.1 [get_clocks clk1]
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup 0.3
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -hold 0.15

# Clock transition
set_clock_transition -rise -max 0.15 [get_clocks clk1]
set_clock_transition -fall -min 0.08 [get_clocks clk1]
set_clock_transition 0.1 [get_clocks clk2]

############################################################
# IO constraints
############################################################

# Input delays with various options
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 -rise -max 2.5 [get_ports in2]
set_input_delay -clock clk1 -fall -min 1.0 [get_ports in2]
set_input_delay -clock clk2 1.8 [get_ports in3]
set_input_delay -clock clk1 -clock_fall 1.5 [get_ports in3] -add_delay

# Output delays with various options
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 -rise -max 3.5 [get_ports out2]
set_output_delay -clock clk2 -fall -min 1.5 [get_ports out2] -add_delay

############################################################
# Driving cell and load
############################################################

set_driving_cell -lib_cell BUF_X1 [get_ports in1]
set_driving_cell -lib_cell INV_X1 -pin ZN [get_ports in2]
set_driving_cell -lib_cell BUF_X4 [get_ports in3]

# set_drive
set_drive 100 [get_ports in1]

set_load 0.05 [get_ports out1]
set_load -pin_load 0.03 [get_ports out2]

# Input transition
set_input_transition 0.15 [get_ports in1]
set_input_transition -rise -max 0.12 [get_ports in2]
set_input_transition -fall -min 0.08 [get_ports in2]

############################################################
# Design limits
############################################################

set_max_transition 0.5 [current_design]
set_max_capacitance 0.2 [current_design]
set_max_fanout 20 [current_design]
set_max_transition 0.3 [get_ports out1]
set_max_capacitance 0.1 [get_ports out1]
set_max_transition -clock_path 0.2 [get_clocks clk1]
set_max_transition -data_path 0.4 [get_clocks clk1]

############################################################
# Exception paths
############################################################

# False path
set_false_path -from [get_clocks clk1] -to [get_clocks clk2]
set_false_path -from [get_ports in1] -through [get_pins and1/ZN] -to [get_ports out1]
set_false_path -rise_from [get_ports in3] -fall_to [get_ports out2]

# Multicycle path
set_multicycle_path -setup 2 -from [get_ports in1] -to [get_ports out1]
set_multicycle_path -hold 1 -from [get_ports in1] -to [get_ports out1]

# Max/min delay
set_max_delay -from [get_ports in2] -to [get_ports out1] 8.0
set_min_delay -from [get_ports in2] -to [get_ports out1] 1.0

# Group path
group_path -name group_clk1 -from [get_clocks clk1]
group_path -name group_io -from [get_ports in1] -to [get_ports out1]

############################################################
# Clock groups
############################################################

set_clock_groups -asynchronous -group {clk1 clk1_fast} -group {clk2}

############################################################
# Clock sense
############################################################

set_clock_sense -positive -clocks [get_clocks clk1] [get_pins buf1/Z]

############################################################
# Case analysis and logic values
############################################################

set_case_analysis 0 [get_ports in3]

############################################################
# Operating conditions
############################################################

set_operating_conditions typical

############################################################
# Wire load
############################################################

set_wire_load_model -name "5K_hvratio_1_1"
set_wire_load_mode enclosed

############################################################
# Timing derate
############################################################

set_timing_derate -early 0.95
set_timing_derate -late 1.05

############################################################
# Disable timing
############################################################

set_disable_timing [get_cells buf1]
set_disable_timing [get_lib_cells NangateOpenCellLibrary/INV_X1] -from A -to ZN

############################################################
# Min pulse width
############################################################

set_min_pulse_width 1.0 [get_clocks clk1]

############################################################
# Port external pin cap
############################################################

set_port_fanout_number 4 [get_ports out1]

############################################################
# Resistance
############################################################

set_resistance -min 10.0 [get_nets n1]
set_resistance -max 20.0 [get_nets n1]

############################################################
# set_max_area
############################################################

set_max_area 100.0

############################################################
# Write SDC with various options
############################################################

set sdc_file1 [make_result_file sdc_write_comprehensive1.sdc]
write_sdc -no_timestamp $sdc_file1

set sdc_file2 [make_result_file sdc_write_comprehensive2.sdc]
write_sdc -no_timestamp -digits 6 $sdc_file2

############################################################
# Read back SDC
############################################################

# Read the SDC file (re-applying constraints)
read_sdc $sdc_file1

report_checks
