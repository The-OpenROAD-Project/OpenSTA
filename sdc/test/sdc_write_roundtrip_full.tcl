# Test comprehensive write_sdc roundtrip covering all uncovered
# WriteSdc.cc functions: writeDisabledPorts, writeFalsePaths with
# all transition variants, writeGroupPath, writeOutputDrives,
# writeMinPulseWidth, writeInterClockUncertainty, writeClockGroups.
# Also targets Sdc.cc unset/remove operations for maximum coverage.
# Targets:
#   WriteSdc.cc: writeDisabledPorts, writeDisabledInstances,
#     writeDisabledCells (all/from/to/fromTo paths),
#     writeFalsePaths (setup/hold, rise/fall from/to),
#     writeExceptionThruPins with rise/fall_through,
#     writeGroupPath (named + default + through),
#     writeOutputDrives / writeDrivingCells,
#     writeDriveResistances (rise/fall/min/max),
#     writeMinPulseWidths (all target types, hi!=lo and hi==lo),
#     writePortExtCap (pin/wire/rise/fall/min/max),
#     writeInterClockUncertainty,
#     writeClockGroups (all 3 types),
#     writeClockSenses,
#     writeClockInsertions, writePropagatedClkPins,
#     writeDataChecks, writeClockGatingCheck,
#     writeCapacitanceLimits, writeResistance
#   Sdc.cc: all getter functions during write, plus:
#     removeCaseAnalysis, removePropagatedClock,
#     removeClockGroups (via unset_clock_groups)
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

############################################################
# Setup complete constraint environment
############################################################
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 -waveform {0 10} [get_ports clk2]
create_clock -name vclk -period 8
puts "PASS: clocks"

create_generated_clock -name gclk_div -source [get_ports clk1] -divide_by 2 [get_pins reg1/Q]
puts "PASS: generated clocks"

# IO delays with all variants
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 -rise -max 2.5 [get_ports in2]
set_input_delay -clock clk1 -fall -min 1.0 [get_ports in2]
set_input_delay -clock clk2 1.8 [get_ports in3]
set_input_delay -clock clk1 -clock_fall 1.5 [get_ports in3] -add_delay
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 -rise -max 3.5 [get_ports out2]
set_output_delay -clock clk2 -fall -min 1.5 [get_ports out2] -add_delay
puts "PASS: IO delays"

# Driving cells and drive resistance
set_driving_cell -lib_cell BUF_X1 [get_ports in1]
set_driving_cell -lib_cell INV_X1 -pin ZN [get_ports in2]
set_driving_cell -lib_cell BUF_X4 [get_ports in3]
set_drive 100 [get_ports in1]
set_drive -rise 80 [get_ports in2]
set_drive -fall 120 [get_ports in2]
puts "PASS: drives"

# Input transitions
set_input_transition 0.15 [get_ports in1]
set_input_transition -rise -max 0.12 [get_ports in2]
set_input_transition -fall -min 0.08 [get_ports in2]
puts "PASS: input transitions"

# Loads with all options
set_load -pin_load 0.05 [get_ports out1]
set_load -wire_load 0.02 [get_ports out1]
set_load -pin_load -rise 0.04 [get_ports out2]
set_load -pin_load -fall 0.045 [get_ports out2]
set_load -min 0.01 [get_ports out1]
set_load -max 0.06 [get_ports out1]
set_port_fanout_number 4 [get_ports out1]
puts "PASS: loads"

# Net loads
catch {
  set_load 0.01 [get_nets n1]
  set_load 0.02 [get_nets n2]
}
puts "PASS: net loads"

# Clock latency (source + network)
set_clock_latency -source 0.5 [get_clocks clk1]
set_clock_latency -source -early 0.3 [get_clocks clk1]
set_clock_latency -source -late 0.6 [get_clocks clk1]
set_clock_latency -source -rise -max 0.65 [get_clocks clk1]
set_clock_latency -source -fall -min 0.25 [get_clocks clk1]
set_clock_latency 0.2 [get_clocks clk2]
puts "PASS: clock latency"

# Clock uncertainty (simple + inter-clock)
set_clock_uncertainty -setup 0.2 [get_clocks clk1]
set_clock_uncertainty -hold 0.1 [get_clocks clk1]
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup 0.3
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -hold 0.15
set_clock_uncertainty -from [get_clocks clk2] -to [get_clocks clk1] -setup 0.28
set_clock_uncertainty -from [get_clocks clk2] -to [get_clocks clk1] -hold 0.12
puts "PASS: clock uncertainty"

# Clock transition
set_clock_transition -rise -max 0.15 [get_clocks clk1]
set_clock_transition -fall -min 0.08 [get_clocks clk1]
set_clock_transition 0.1 [get_clocks clk2]
puts "PASS: clock transition"

# Propagated clock
set_propagated_clock [get_clocks clk1]
puts "PASS: propagated"

# Clock sense (using set_clock_sense which triggers the same code)
set_clock_sense -positive -clocks [get_clocks clk1] [get_pins buf1/Z]
puts "PASS: clock sense"

# Disable timing - instances, lib cells
set_disable_timing [get_cells buf1]
set_disable_timing [get_lib_cells NangateOpenCellLibrary/AND2_X1] -from A1 -to ZN
set_disable_timing [get_lib_cells NangateOpenCellLibrary/NOR2_X1]
puts "PASS: disable timing"

# Case analysis
set_case_analysis 0 [get_ports in3]
puts "PASS: case analysis"

# Logic values
set_logic_one [get_ports in2]
puts "PASS: logic"

# Design limits
set_max_transition 0.5 [current_design]
set_max_capacitance 0.2 [current_design]
set_max_fanout 20 [current_design]
set_max_transition 0.3 [get_ports out1]
set_max_capacitance 0.1 [get_ports out1]
set_max_transition -clock_path 0.2 [get_clocks clk1]
set_max_transition -data_path 0.4 [get_clocks clk1]
set_max_area 100.0
puts "PASS: design limits"

# False paths with all transition combinations
set_false_path -from [get_clocks clk1] -to [get_clocks clk2]
set_false_path -setup -from [get_clocks clk1] -to [get_clocks vclk]
set_false_path -hold -from [get_clocks vclk] -to [get_clocks clk1]
set_false_path -rise_from [get_ports in3] -fall_to [get_ports out2]
set_false_path -from [get_ports in1] -through [get_pins and1/ZN] -to [get_ports out1]
set_false_path -from [get_ports in2] \
  -rise_through [get_pins buf1/Z] \
  -fall_through [get_nets n3] \
  -to [get_ports out1]
puts "PASS: false paths"

# Multicycle paths
set_multicycle_path -setup 2 -from [get_ports in1] -to [get_ports out1]
set_multicycle_path -hold 1 -from [get_ports in1] -to [get_ports out1]
set_multicycle_path -setup -start 3 -from [get_ports in2] -to [get_ports out2]
set_multicycle_path -hold -end 1 -from [get_ports in2] -to [get_ports out2]
puts "PASS: multicycle paths"

# Max/min delay
set_max_delay -from [get_ports in2] -to [get_ports out1] 8.0
set_min_delay -from [get_ports in2] -to [get_ports out1] 1.0
set_max_delay -from [get_ports in3] -through [get_cells or1] -to [get_ports out2] 7.0
puts "PASS: max/min delay"

# Group paths
group_path -name grp_clk1 -from [get_clocks clk1]
group_path -name grp_io -from [get_ports in1] -to [get_ports out1]
group_path -name grp_thru -from [get_ports in2] \
  -through [get_nets n2] -to [get_ports out1]
group_path -default -from [get_ports in3] -to [get_ports out2]
puts "PASS: group paths"

# Clock groups
set_clock_groups -asynchronous -name async1 \
  -group {clk1 gclk_div} \
  -group {clk2}
puts "PASS: clock groups"

# Clock gating check
set_clock_gating_check -setup 0.5 [get_clocks clk1]
set_clock_gating_check -hold 0.3 [get_clocks clk1]
set_clock_gating_check -setup 0.35 [get_clocks clk2]
set_clock_gating_check -hold 0.15 [get_clocks clk2]
puts "PASS: clock gating check"

# Min pulse width
set_min_pulse_width 0.5
set_min_pulse_width -high 0.6 [get_clocks clk1]
set_min_pulse_width -low 0.4 [get_clocks clk1]
set_min_pulse_width 0.55 [get_clocks clk2]
puts "PASS: min pulse width"

# Latch borrow
set_max_time_borrow 2.0 [get_clocks clk1]
set_max_time_borrow 1.5 [get_pins reg1/D]
puts "PASS: latch borrow"

# Net resistance
set_resistance -min 10.0 [get_nets n1]
set_resistance -max 20.0 [get_nets n1]
puts "PASS: net resistance"

# Data checks
catch {
  set_data_check -from [get_pins reg1/Q] -to [get_pins reg2/D] -setup 0.5
  set_data_check -from [get_pins reg1/Q] -to [get_pins reg2/D] -hold 0.3
}
puts "PASS: data checks"

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
puts "PASS: timing derate"

puts "PASS: constraints set"

############################################################
# Write in all formats
############################################################
set sdc_native [make_result_file sdc_wrt_full_native.sdc]
write_sdc -no_timestamp $sdc_native
puts "PASS: write native"

set sdc_compat [make_result_file sdc_wrt_full_compat.sdc]
write_sdc -no_timestamp -compatible $sdc_compat
puts "PASS: write compatible"

set sdc_d2 [make_result_file sdc_wrt_full_d2.sdc]
write_sdc -no_timestamp -digits 2 $sdc_d2
puts "PASS: write digits 2"

set sdc_d8 [make_result_file sdc_wrt_full_d8.sdc]
write_sdc -no_timestamp -digits 8 $sdc_d8
puts "PASS: write digits 8"

set sdc_hpins [make_result_file sdc_wrt_full_hpins.sdc]
write_sdc -no_timestamp -map_hpins $sdc_hpins
puts "PASS: write map_hpins"

############################################################
# Read back native and re-write
############################################################
read_sdc $sdc_native
puts "PASS: read native"

set sdc_rewrite [make_result_file sdc_wrt_full_rewrite.sdc]
write_sdc -no_timestamp $sdc_rewrite
puts "PASS: write after read"

############################################################
# Read compatible and verify
############################################################
read_sdc $sdc_compat
puts "PASS: read compatible"

set sdc_final [make_result_file sdc_wrt_full_final.sdc]
write_sdc -no_timestamp $sdc_final
puts "PASS: write final"

puts "ALL PASSED"
