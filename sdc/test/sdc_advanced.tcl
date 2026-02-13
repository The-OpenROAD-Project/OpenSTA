# Test advanced SDC commands for code coverage
# Targets: DeratingFactors.cc, DisabledPorts.cc, InputDrive.cc,
#          ClockInsertion.cc, ClockLatency.cc, Sdc.cc, DataCheck.cc
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

# Setup clocks
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_input_delay -clock clk1 2.0 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]
puts "PASS: basic setup"

############################################################
# Timing derate - comprehensive (DeratingFactors.cc)
############################################################

# Global early/late
set_timing_derate -early 0.95
set_timing_derate -late 1.05
puts "PASS: global timing derate"

# Rise/fall derate
set_timing_derate -early -rise 0.96
set_timing_derate -late -fall 1.04
puts "PASS: rise/fall timing derate"

# Data path derate
set_timing_derate -early -data 0.94
set_timing_derate -late -data 1.06
puts "PASS: data path derate"

# Clock path derate
set_timing_derate -early -clock 0.97
set_timing_derate -late -clock 1.03
puts "PASS: clock path derate"

# Cell type derate
set_timing_derate -early -cell_delay 0.93
set_timing_derate -late -cell_delay 1.07
puts "PASS: cell_delay derate"

set_timing_derate -early -net_delay 0.92
set_timing_derate -late -net_delay 1.08
puts "PASS: net_delay derate"

# Cell-specific derate (value first, then object)
set_timing_derate -early -cell_delay 0.91 [get_lib_cells NangateOpenCellLibrary/INV_X1]
set_timing_derate -late -cell_delay 1.09 [get_lib_cells NangateOpenCellLibrary/INV_X1]
puts "PASS: cell-specific derate"

# Instance-specific derate
set_timing_derate -early -cell_delay 0.90 [get_cells buf1]
set_timing_derate -late -cell_delay 1.10 [get_cells buf1]
puts "PASS: instance-specific derate"

report_checks
puts "PASS: report_checks after derate"

# Unset all derating
unset_timing_derate
puts "PASS: unset_timing_derate"

############################################################
# Disable timing - comprehensive (DisabledPorts.cc, Sdc.cc)
############################################################

# Disable timing on instance
set_disable_timing [get_cells buf1]
puts "PASS: disable timing instance buf1"

report_checks
puts "PASS: report after disable buf1"

unset_disable_timing [get_cells buf1]
puts "PASS: unset_disable_timing buf1"

# Disable on pin
set_disable_timing [get_pins buf1/A]
puts "PASS: disable timing pin buf1/A"

unset_disable_timing [get_pins buf1/A]
puts "PASS: unset disable pin buf1/A"

# Disable on lib cell from/to
set_disable_timing [get_lib_cells NangateOpenCellLibrary/BUF_X1] -from A -to Z
puts "PASS: disable lib cell from/to"

unset_disable_timing [get_lib_cells NangateOpenCellLibrary/BUF_X1] -from A -to Z
puts "PASS: unset disable lib cell from/to"

# Disable on lib cell (all arcs)
set_disable_timing [get_lib_cells NangateOpenCellLibrary/INV_X1]
puts "PASS: disable lib cell all arcs"

unset_disable_timing [get_lib_cells NangateOpenCellLibrary/INV_X1]
puts "PASS: unset disable lib cell all arcs"

############################################################
# Input drive - comprehensive (InputDrive.cc)
############################################################

# set_driving_cell with various options
set_driving_cell -lib_cell BUF_X1 [get_ports in1]
puts "PASS: set_driving_cell BUF_X1"

set_driving_cell -lib_cell INV_X1 -pin ZN [get_ports in2]
puts "PASS: set_driving_cell INV_X1 -pin ZN"

set_driving_cell -lib_cell BUF_X4 -rise [get_ports in3]
puts "PASS: set_driving_cell -rise"

set_driving_cell -lib_cell BUF_X2 -fall [get_ports in3]
puts "PASS: set_driving_cell -fall"

# set_drive (resistance-based drive)
set_drive 100 [get_ports in1]
puts "PASS: set_drive 100"

set_drive -rise 80 [get_ports in2]
puts "PASS: set_drive -rise 80"

set_drive -fall 120 [get_ports in2]
puts "PASS: set_drive -fall 120"

set_drive 0 [get_ports in1]
puts "PASS: set_drive 0 (ideal)"

# Input transition
set_input_transition 0.15 [get_ports in1]
set_input_transition -rise -max 0.12 [get_ports in2]
set_input_transition -fall -min 0.08 [get_ports in2]
set_input_transition -rise -min 0.06 [get_ports in3]
set_input_transition -fall -max 0.18 [get_ports in3]
puts "PASS: input transitions"

############################################################
# Clock insertion delay (ClockInsertion.cc) via set_clock_latency -source
############################################################

# set_clock_latency -source exercises ClockInsertion.cc
set_clock_latency -source 0.5 [get_clocks clk1]
puts "PASS: clock insertion (source latency)"

set_clock_latency -source -rise -max 0.6 [get_clocks clk1]
puts "PASS: clock insertion -rise -max"

set_clock_latency -source -fall -min 0.3 [get_clocks clk1]
puts "PASS: clock insertion -fall -min"

set_clock_latency -source -rise -min 0.2 [get_clocks clk1]
puts "PASS: clock insertion -rise -min"

set_clock_latency -source -fall -max 0.7 [get_clocks clk1]
puts "PASS: clock insertion -fall -max"

# Source latency with -early/-late
set_clock_latency -source -early 0.4 [get_clocks clk2]
puts "PASS: clock insertion -early"

set_clock_latency -source -late 0.6 [get_clocks clk2]
puts "PASS: clock insertion -late"

############################################################
# Clock latency with more options (ClockLatency.cc)
############################################################

set_clock_latency -source 0.5 [get_clocks clk2]
set_clock_latency -source -rise -min 0.3 [get_clocks clk2]
set_clock_latency -source -rise -max 0.7 [get_clocks clk2]
set_clock_latency -source -fall -min 0.2 [get_clocks clk2]
set_clock_latency -source -fall -max 0.6 [get_clocks clk2]
puts "PASS: source latency with all corners"

set_clock_latency 0.3 [get_clocks clk1]
set_clock_latency -rise -min 0.2 [get_clocks clk1]
set_clock_latency -rise -max 0.4 [get_clocks clk1]
set_clock_latency -fall -min 0.1 [get_clocks clk1]
set_clock_latency -fall -max 0.5 [get_clocks clk1]
puts "PASS: network latency with all corners"

############################################################
# Port external capacitance (PortExtCap.cc)
############################################################

set_load 0.05 [get_ports out1]
set_load -pin_load 0.03 [get_ports out2]
set_load -wire_load 0.02 [get_ports out1]
set_load -min 0.01 [get_ports out1]
set_load -max 0.06 [get_ports out1]
puts "PASS: set_load various options"

############################################################
# Net resistance / capacitance (Sdc.cc)
############################################################

set_resistance -min 10.0 [get_nets n1]
set_resistance -max 20.0 [get_nets n1]
puts "PASS: set_resistance"

############################################################
# Clock gating check (Sdc.cc)
############################################################

set_clock_gating_check -setup 0.5 [get_clocks clk1]
set_clock_gating_check -hold 0.3 [get_clocks clk1]
puts "PASS: clock gating check on clock"

set_clock_gating_check -setup 0.4 [current_design]
set_clock_gating_check -hold 0.2 [current_design]
puts "PASS: clock gating check on design"

############################################################
# Latch borrow limit
############################################################

set_max_time_borrow 2.0 [get_clocks clk1]
puts "PASS: set_max_time_borrow clock"

set_max_time_borrow 1.5 [get_pins reg1/D]
puts "PASS: set_max_time_borrow pin"

############################################################
# Data check
############################################################

catch {
  set_data_check -from [get_pins reg1/Q] -to [get_pins reg2/D] -setup 0.5
  puts "PASS: set_data_check -setup"
}

catch {
  set_data_check -from [get_pins reg1/Q] -to [get_pins reg2/D] -hold 0.3
  puts "PASS: set_data_check -hold"
}

############################################################
# set_ideal_network / set_ideal_transition
############################################################

set_ideal_network [get_ports clk1]
puts "PASS: set_ideal_network"

catch {
  set_ideal_transition 0.0 [get_ports clk1]
  puts "PASS: set_ideal_transition"
}

############################################################
# Wire load mode (various modes)
############################################################

set_wire_load_mode top
puts "PASS: set_wire_load_mode top"

set_wire_load_mode enclosed
puts "PASS: set_wire_load_mode enclosed"

set_wire_load_mode segmented
puts "PASS: set_wire_load_mode segmented"

set_wire_load_model -name "1K_hvratio_1_1"
puts "PASS: set_wire_load_model 1K"

set_wire_load_model -name "5K_hvratio_1_1"
puts "PASS: set_wire_load_model 5K"

############################################################
# Min pulse width (Sdc.cc)
############################################################

set_min_pulse_width 0.8 [get_clocks clk1]
puts "PASS: set_min_pulse_width clock"

set_min_pulse_width -high 0.5 [get_clocks clk1]
puts "PASS: set_min_pulse_width -high"

set_min_pulse_width -low 0.4 [get_clocks clk1]
puts "PASS: set_min_pulse_width -low"

############################################################
# Final report
############################################################

report_checks
puts "PASS: final report_checks"

report_check_types -max_capacitance -max_slew -max_fanout
puts "PASS: report_check_types"

puts "ALL PASSED"
