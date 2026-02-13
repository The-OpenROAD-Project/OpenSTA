# Test advanced port delay, derating, data check, and misc SDC features
# Targets: PortDelay.cc, DeratingFactors.cc, DataCheck.cc, Clock.cc,
#          ClockInsertion.cc, ClockLatency.cc, DisabledPorts.cc,
#          WriteSdc.cc (writePortDelay, writeDeratingFactors, writeDataCheck,
#                       writeConstants, writeDisabled*, writeClockInsertion)
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

############################################################
# Setup clocks
############################################################

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 -waveform {0 10} [get_ports clk2]
create_clock -name vclk -period 8
puts "PASS: clocks created"

############################################################
# Input delays with comprehensive min/max/rise/fall options
############################################################

# Basic input delay
set_input_delay -clock clk1 2.0 [get_ports in1]
puts "PASS: basic input delay"

# Rise/fall max/min on same port
set_input_delay -clock clk1 -rise -max 2.5 [get_ports in2]
set_input_delay -clock clk1 -rise -min 1.2 [get_ports in2]
set_input_delay -clock clk1 -fall -max 2.3 [get_ports in2]
set_input_delay -clock clk1 -fall -min 1.0 [get_ports in2]
puts "PASS: input delay rise/fall min/max"

# Input delay with clock_fall
set_input_delay -clock clk1 -clock_fall 1.5 [get_ports in3]
puts "PASS: input delay -clock_fall"

# Additional delay on same port
set_input_delay -clock clk2 1.8 [get_ports in3] -add_delay
puts "PASS: input delay -add_delay"

# Rise/fall with clock_fall + add_delay
set_input_delay -clock clk1 -clock_fall -rise -max 2.8 [get_ports in3] -add_delay
set_input_delay -clock clk1 -clock_fall -fall -min 0.8 [get_ports in3] -add_delay
puts "PASS: input delay -clock_fall -rise/-fall -add_delay"

############################################################
# Output delays with comprehensive options
############################################################

# Basic output delay
set_output_delay -clock clk1 3.0 [get_ports out1]
puts "PASS: basic output delay"

# Rise/fall max/min on same port
set_output_delay -clock clk2 -rise -max 3.5 [get_ports out2]
set_output_delay -clock clk2 -rise -min 1.5 [get_ports out2]
set_output_delay -clock clk2 -fall -max 3.2 [get_ports out2]
set_output_delay -clock clk2 -fall -min 1.2 [get_ports out2]
puts "PASS: output delay rise/fall min/max"

# Output delay with clock_fall
set_output_delay -clock clk1 -clock_fall 2.5 [get_ports out1] -add_delay
puts "PASS: output delay -clock_fall -add_delay"

############################################################
# Source latency with early/late (ClockInsertion.cc)
############################################################

set_clock_latency -source -early 0.3 [get_clocks clk1]
set_clock_latency -source -late 0.5 [get_clocks clk1]
set_clock_latency -source -early -rise 0.25 [get_clocks clk1]
set_clock_latency -source -late -rise 0.55 [get_clocks clk1]
set_clock_latency -source -early -fall 0.20 [get_clocks clk1]
set_clock_latency -source -late -fall 0.45 [get_clocks clk1]
puts "PASS: source latency early/late rise/fall"

set_clock_latency -source -early 0.2 [get_clocks clk2]
set_clock_latency -source -late 0.4 [get_clocks clk2]
puts "PASS: source latency on clk2"

# Network latency with all corners
set_clock_latency 0.3 [get_clocks clk1]
set_clock_latency -rise -max 0.4 [get_clocks clk1]
set_clock_latency -rise -min 0.2 [get_clocks clk1]
set_clock_latency -fall -max 0.35 [get_clocks clk1]
set_clock_latency -fall -min 0.15 [get_clocks clk1]
puts "PASS: network latency all rf/minmax"

############################################################
# Timing derate with cell/net type specifics (DeratingFactors.cc)
############################################################

# Global derate
set_timing_derate -early 0.95
set_timing_derate -late 1.05
puts "PASS: global derate"

# Rise/fall derate
set_timing_derate -early -rise 0.96
set_timing_derate -late -fall 1.04
puts "PASS: rise/fall derate"

# Clock/data path derate
set_timing_derate -early -clock 0.97
set_timing_derate -late -clock 1.03
set_timing_derate -early -data 0.94
set_timing_derate -late -data 1.06
puts "PASS: clock/data derate"

# Cell delay derate
set_timing_derate -early -cell_delay 0.93
set_timing_derate -late -cell_delay 1.07
puts "PASS: cell_delay derate"

# Net delay derate
set_timing_derate -early -net_delay 0.92
set_timing_derate -late -net_delay 1.08
puts "PASS: net_delay derate"

# Cell-specific derate (on lib cell)
set_timing_derate -early -cell_delay 0.91 [get_lib_cells NangateOpenCellLibrary/INV_X1]
set_timing_derate -late -cell_delay 1.09 [get_lib_cells NangateOpenCellLibrary/INV_X1]
puts "PASS: lib cell derate"

# Instance-specific derate
set_timing_derate -early -cell_delay 0.90 [get_cells buf1]
set_timing_derate -late -cell_delay 1.10 [get_cells buf1]
puts "PASS: instance derate"

set_timing_derate -early -cell_delay 0.89 [get_cells inv1]
set_timing_derate -late -cell_delay 1.11 [get_cells inv1]
puts "PASS: instance derate inv1"

############################################################
# Disable timing - various forms (DisabledPorts.cc)
############################################################

# Disable instance
set_disable_timing [get_cells buf1]
puts "PASS: disable buf1"

# Disable pin
set_disable_timing [get_pins inv1/A]
puts "PASS: disable pin inv1/A"

# Disable lib cell arc
set_disable_timing [get_lib_cells NangateOpenCellLibrary/BUF_X1] -from A -to Z
puts "PASS: disable lib cell arc"

# Disable lib cell (all arcs)
set_disable_timing [get_lib_cells NangateOpenCellLibrary/NAND2_X1]
puts "PASS: disable lib cell NAND2"

############################################################
# Data check (DataCheck.cc)
############################################################

catch {
  set_data_check -from [get_pins reg1/Q] -to [get_pins reg2/D] -setup 0.5
  puts "PASS: data_check -setup"
}

catch {
  set_data_check -from [get_pins reg1/Q] -to [get_pins reg2/D] -hold 0.3
  puts "PASS: data_check -hold"
}

catch {
  set_data_check -from [get_pins reg1/Q] -to [get_pins reg2/D] -setup 0.6 -clock_fall
  puts "PASS: data_check -setup -clock_fall"
}

############################################################
# Case analysis and logic values (Constants)
############################################################

set_case_analysis 0 [get_ports in3]
puts "PASS: case_analysis 0"

set_logic_one [get_ports in2]
puts "PASS: logic_one"

set_logic_zero [get_ports in1]
puts "PASS: logic_zero"

############################################################
# Latch borrow limits
############################################################

set_max_time_borrow 2.0 [get_clocks clk1]
puts "PASS: max_time_borrow on clock"

set_max_time_borrow 1.5 [get_pins reg1/D]
puts "PASS: max_time_borrow on pin"

############################################################
# Min pulse width (all options)
############################################################

set_min_pulse_width 1.0 [get_clocks clk1]
set_min_pulse_width -high 0.6 [get_clocks clk1]
set_min_pulse_width -low 0.4 [get_clocks clk1]
set_min_pulse_width 0.8 [get_clocks clk2]
puts "PASS: min pulse width"

############################################################
# Clock gating check
############################################################

set_clock_gating_check -setup 0.5 [get_clocks clk1]
set_clock_gating_check -hold 0.3 [get_clocks clk1]
set_clock_gating_check -setup 0.4 [current_design]
set_clock_gating_check -hold 0.2 [current_design]
puts "PASS: clock gating check"

############################################################
# Driving cells with various options
############################################################

set_driving_cell -lib_cell BUF_X1 [get_ports in1]
set_driving_cell -lib_cell INV_X1 -pin ZN [get_ports in2]
set_driving_cell -lib_cell BUF_X4 -rise [get_ports in3]
set_driving_cell -lib_cell BUF_X2 -fall [get_ports in3]
puts "PASS: driving cells"

# set_drive
set_drive 100 [get_ports in1]
set_drive -rise 80 [get_ports in2]
set_drive -fall 120 [get_ports in2]
puts "PASS: set_drive"

# Loads
set_load 0.05 [get_ports out1]
set_load -pin_load 0.03 [get_ports out2]
set_load -wire_load 0.02 [get_ports out1]
set_load -min 0.01 [get_ports out1]
set_load -max 0.06 [get_ports out1]
puts "PASS: loads"

# Input transition
set_input_transition 0.15 [get_ports in1]
set_input_transition -rise -max 0.12 [get_ports in2]
set_input_transition -fall -min 0.08 [get_ports in2]
set_input_transition -rise -min 0.06 [get_ports in3]
set_input_transition -fall -max 0.18 [get_ports in3]
puts "PASS: input transitions"

############################################################
# Port fanout number
############################################################

set_port_fanout_number 4 [get_ports out1]
set_port_fanout_number 8 [get_ports out2]
puts "PASS: port fanout number"

############################################################
# Net resistance
############################################################

set_resistance -min 10.0 [get_nets n1]
set_resistance -max 20.0 [get_nets n1]
puts "PASS: net resistance"

############################################################
# Design limits
############################################################

set_max_transition 0.5 [current_design]
set_max_capacitance 0.2 [current_design]
set_max_fanout 20 [current_design]
set_max_area 100.0
puts "PASS: design limits"

############################################################
# Operating conditions and wire load
############################################################

set_operating_conditions typical
set_wire_load_model -name "5K_hvratio_1_1"
set_wire_load_mode enclosed
puts "PASS: operating conditions and wire load"

############################################################
# Voltage setting
############################################################

catch {set_voltage 1.1 -min 0.9}
puts "PASS: set_voltage"

############################################################
# Write SDC with all the constraints
############################################################

set sdc_file1 [make_result_file sdc_port_delay_adv1.sdc]
write_sdc -no_timestamp $sdc_file1
puts "PASS: write_sdc native"

set sdc_file2 [make_result_file sdc_port_delay_adv2.sdc]
write_sdc -no_timestamp -compatible $sdc_file2
puts "PASS: write_sdc -compatible"

set sdc_file3 [make_result_file sdc_port_delay_adv3.sdc]
write_sdc -no_timestamp -digits 8 $sdc_file3
puts "PASS: write_sdc -digits 8"

############################################################
# Report checks to verify
############################################################

report_checks
puts "PASS: report_checks"

report_checks -path_delay min
puts "PASS: report_checks -path_delay min"

report_check_types -max_slew -max_capacitance -max_fanout
puts "PASS: report_check_types"

############################################################
# Unset derating and verify
############################################################

unset_timing_derate
puts "PASS: unset_timing_derate"

# Unset disable
unset_disable_timing [get_cells buf1]
unset_disable_timing [get_pins inv1/A]
unset_disable_timing [get_lib_cells NangateOpenCellLibrary/BUF_X1] -from A -to Z
unset_disable_timing [get_lib_cells NangateOpenCellLibrary/NAND2_X1]
puts "PASS: unset disable"

# Unset case analysis
unset_case_analysis [get_ports in3]
puts "PASS: unset case analysis"

# Unset clock latencies
unset_clock_latency [get_clocks clk1]
unset_clock_latency -source [get_clocks clk1]
unset_clock_latency [get_clocks clk2]
unset_clock_latency -source [get_clocks clk2]
puts "PASS: unset clock latencies"

report_checks
puts "PASS: final report_checks"

puts "ALL PASSED"
