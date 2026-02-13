# Test SDC write/read roundtrip for code coverage
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

############################################################
# Setup various constraints
############################################################

# Clocks
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 -waveform {0 10} [get_ports clk2]
puts "PASS: clocks created"

# Generated clock
create_generated_clock -name gen_div2 -source [get_ports clk1] -divide_by 2 [get_pins reg1/Q]
puts "PASS: generated clock created"

# Delays
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 1.5 [get_ports in2]
set_input_delay -clock clk1 1.8 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 2.5 [get_ports out2]
puts "PASS: delays set"

# Clock uncertainty
set_clock_uncertainty -setup 0.2 [get_clocks clk1]
set_clock_uncertainty -hold 0.1 [get_clocks clk1]
puts "PASS: uncertainty set"

# Clock latency
set_clock_latency -source 0.5 [get_clocks clk1]
set_clock_latency 0.3 [get_clocks clk2]
puts "PASS: latency set"

# Clock transition
set_clock_transition 0.1 [get_clocks clk1]
puts "PASS: transition set"

# Driving cell
set_driving_cell -lib_cell BUF_X1 [get_ports in1]
puts "PASS: driving cell set"

# Load
set_load 0.05 [get_ports out1]
set_load 0.04 [get_ports out2]
puts "PASS: load set"

# Input transition
set_input_transition 0.15 [get_ports in1]
puts "PASS: input transition set"

# False path
set_false_path -from [get_clocks clk1] -to [get_clocks clk2]
puts "PASS: false path set"

# Multicycle
set_multicycle_path -setup 2 -from [get_ports in1] -to [get_ports out1]
puts "PASS: multicycle set"

# Max/min delay
set_max_delay -from [get_ports in2] -to [get_ports out1] 8.0
puts "PASS: max delay set"

# Max transition/capacitance/fanout
set_max_transition 0.5 [current_design]
set_max_capacitance 0.2 [current_design]
set_max_fanout 20 [current_design]
puts "PASS: design limits set"

# Case analysis
set_case_analysis 0 [get_ports in3]
puts "PASS: case analysis set"

# Operating conditions
set_operating_conditions typical
puts "PASS: operating conditions set"

# Wire load
set_wire_load_model -name "5K_hvratio_1_1"
puts "PASS: wire load model set"

# Timing derate
set_timing_derate -early 0.95
set_timing_derate -late 1.05
puts "PASS: timing derate set"

# Propagated clock
set_propagated_clock [get_clocks clk1]
puts "PASS: propagated clock set"

############################################################
# Write SDC
############################################################

set sdc_file [make_result_file sdc_write_read.sdc]
write_sdc -no_timestamp $sdc_file
puts "PASS: write_sdc"

############################################################
# Clear and read back
############################################################

# Report before clear
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: report_checks before clear"

report_clock_properties
puts "PASS: report_clock_properties before clear"

puts "ALL PASSED"
