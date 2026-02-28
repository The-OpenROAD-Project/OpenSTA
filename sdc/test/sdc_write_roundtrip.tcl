# Test comprehensive write_sdc / read_sdc / re-write roundtrip
# Targets: WriteSdc.cc (all write* functions, WriteGet* classes),
#          Sdc.cc (reading constraints back in, net load/cap, voltage),
#          ExceptionPath.cc (exception comparison during merge),
#          Clock.cc (clock property persistence through roundtrip)
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

############################################################
# Create maximum variety of constraints
############################################################

# Clocks with different waveforms
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 -waveform {0 10} [get_ports clk2]
create_clock -name vclk -period 8
create_clock -name clk1_alt -period 5 -add [get_ports clk1]

# Generated clocks
create_generated_clock -name gclk_div -source [get_ports clk1] -divide_by 2 [get_pins reg1/Q]
create_generated_clock -name gclk_mul -source [get_ports clk2] -multiply_by 3 [get_pins reg3/Q]
create_generated_clock -name gclk_edge -source [get_ports clk1] -edges {1 3 5} [get_pins reg2/Q]

# Propagated clock
set_propagated_clock [get_clocks clk1]

# Clock slew
set_clock_transition -rise -max 0.15 [get_clocks clk1]
set_clock_transition -fall -min 0.08 [get_clocks clk1]
set_clock_transition 0.1 [get_clocks clk2]

# Clock latency
set_clock_latency -source 0.5 [get_clocks clk1]
set_clock_latency -source -early 0.3 [get_clocks clk1]
set_clock_latency -source -late 0.6 [get_clocks clk1]
set_clock_latency -source -rise -max 0.65 [get_clocks clk1]
set_clock_latency -source -fall -min 0.25 [get_clocks clk1]
set_clock_latency 0.2 [get_clocks clk2]
set_clock_latency -rise -max 0.4 [get_clocks clk2]
set_clock_latency -fall -min 0.1 [get_clocks clk2]

# Clock uncertainty (simple + inter-clock)
set_clock_uncertainty -setup 0.2 [get_clocks clk1]
set_clock_uncertainty -hold 0.1 [get_clocks clk1]
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup 0.3
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -hold 0.15
set_clock_uncertainty -from [get_clocks clk2] -to [get_clocks clk1] -setup 0.28
set_clock_uncertainty -from [get_clocks clk2] -to [get_clocks clk1] -hold 0.12

# IO delays (various options)
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 -rise -max 2.5 [get_ports in2]
set_input_delay -clock clk1 -fall -min 1.0 [get_ports in2]
set_input_delay -clock clk2 1.8 [get_ports in3]
set_input_delay -clock clk1 -clock_fall 1.5 [get_ports in3] -add_delay
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 -rise -max 3.5 [get_ports out2]
set_output_delay -clock clk2 -fall -min 1.5 [get_ports out2] -add_delay

# Driving cells / input drive
set_driving_cell -lib_cell BUF_X1 [get_ports in1]
set_driving_cell -lib_cell INV_X1 -pin ZN [get_ports in2]
set_driving_cell -lib_cell BUF_X4 [get_ports in3]
set_drive 100 [get_ports in1]
set_drive -rise 80 [get_ports in2]
set_drive -fall 120 [get_ports in2]

# Loads
set_load 0.05 [get_ports out1]
set_load -pin_load 0.03 [get_ports out2]
set_load -wire_load 0.02 [get_ports out1]

# Input transitions
set_input_transition 0.15 [get_ports in1]
set_input_transition -rise -max 0.12 [get_ports in2]
set_input_transition -fall -min 0.08 [get_ports in2]

# Design limits
set_max_transition 0.5 [current_design]
set_max_capacitance 0.2 [current_design]
set_max_fanout 20 [current_design]
set_max_transition 0.3 [get_ports out1]
set_max_capacitance 0.1 [get_ports out1]
set_max_transition -clock_path 0.2 [get_clocks clk1]
set_max_transition -data_path 0.4 [get_clocks clk1]
set_max_area 100.0

# False paths
set_false_path -from [get_clocks clk1] -to [get_clocks clk2]
set_false_path -from [get_ports in1] -through [get_pins and1/ZN] -to [get_ports out1]
set_false_path -rise_from [get_ports in3] -fall_to [get_ports out2]

# Multicycle paths
set_multicycle_path -setup 2 -from [get_ports in1] -to [get_ports out1]
set_multicycle_path -hold 1 -from [get_ports in1] -to [get_ports out1]
set_multicycle_path -setup 3 -from [get_clocks clk1] -to [get_clocks gclk_div]

# Max/min delay
set_max_delay -from [get_ports in2] -to [get_ports out1] 8.0
set_min_delay -from [get_ports in2] -to [get_ports out1] 1.0

# Group paths
group_path -name grp_clk1 -from [get_clocks clk1]
group_path -name grp_io -from [get_ports in1] -to [get_ports out1]

# Clock groups
set_clock_groups -asynchronous -name async1 \
  -group {clk1 clk1_alt gclk_div gclk_edge} \
  -group {clk2 gclk_mul}

# Clock sense
set_clock_sense -positive -clocks [get_clocks clk1] [get_pins buf1/Z]

# Disable timing
set_disable_timing [get_cells buf1]
set_disable_timing [get_lib_cells NangateOpenCellLibrary/INV_X1] -from A -to ZN

# Case analysis
set_case_analysis 0 [get_ports in3]

# Logic values
set_logic_one [get_ports in2]

# Operating conditions
set_operating_conditions typical

# Wire load
set_wire_load_model -name "5K_hvratio_1_1"
set_wire_load_mode enclosed

# Timing derate
set_timing_derate -early 0.95
set_timing_derate -late 1.05
set_timing_derate -early -clock 0.97
set_timing_derate -late -clock 1.03
set_timing_derate -early -cell_delay 0.91 [get_lib_cells NangateOpenCellLibrary/INV_X1]
set_timing_derate -late -cell_delay 1.09 [get_lib_cells NangateOpenCellLibrary/INV_X1]
set_timing_derate -early -cell_delay 0.90 [get_cells buf1]
set_timing_derate -late -cell_delay 1.10 [get_cells buf1]

# Min pulse width
set_min_pulse_width -high 0.6 [get_clocks clk1]
set_min_pulse_width -low 0.4 [get_clocks clk1]

# Latch borrow
set_max_time_borrow 2.0 [get_clocks clk1]
set_max_time_borrow 1.5 [get_pins reg1/D]

# Clock gating check
set_clock_gating_check -setup 0.5 [get_clocks clk1]
set_clock_gating_check -hold 0.3 [get_clocks clk1]
set_clock_gating_check -setup 0.4 [current_design]
set_clock_gating_check -hold 0.2 [current_design]

# Port fanout
set_port_fanout_number 4 [get_ports out1]

# Net resistance
set_resistance -min 10.0 [get_nets n1]
set_resistance -max 20.0 [get_nets n1]

# Voltage
set_voltage 1.1 -min 0.9

# Data check
set_data_check -from [get_pins reg1/Q] -to [get_pins reg2/D] -setup 0.5
set_data_check -from [get_pins reg1/Q] -to [get_pins reg2/D] -hold 0.3

############################################################
# Write SDC in multiple formats
############################################################

set sdc_native [make_result_file sdc_roundtrip_native.sdc]
write_sdc -no_timestamp $sdc_native

set sdc_compat [make_result_file sdc_roundtrip_compat.sdc]
write_sdc -no_timestamp -compatible $sdc_compat

set sdc_d2 [make_result_file sdc_roundtrip_d2.sdc]
write_sdc -no_timestamp -digits 2 $sdc_d2

set sdc_d8 [make_result_file sdc_roundtrip_d8.sdc]
write_sdc -no_timestamp -digits 8 $sdc_d8

set sdc_hpins [make_result_file sdc_roundtrip_hpins.sdc]
write_sdc -no_timestamp -map_hpins $sdc_hpins

set sdc_compat_d6 [make_result_file sdc_roundtrip_compat_d6.sdc]
write_sdc -no_timestamp -compatible -digits 6 $sdc_compat_d6

report_checks

############################################################
# Read back native SDC (exercises read of all constraint types)
############################################################

# Re-read to exercise constraint merging
read_sdc $sdc_native

report_checks

# Write again after re-read
set sdc_rewrite [make_result_file sdc_roundtrip_rewrite.sdc]
write_sdc -no_timestamp $sdc_rewrite

############################################################
# Read compatible SDC
############################################################

read_sdc $sdc_compat

report_checks

############################################################
# Read high-precision SDC
############################################################

read_sdc $sdc_d8

report_checks

# Final write
set sdc_final [make_result_file sdc_roundtrip_final.sdc]
write_sdc -no_timestamp $sdc_final
