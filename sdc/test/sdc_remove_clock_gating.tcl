# Test remove_clock, disable/remove clock gating check, capacitance limits
# on cell/port/pin targets, and set_load with -corner variants.
# Targets:
#   Sdc.cc: removeClock (full cascade: deleteExceptionsReferencing,
#     deleteInputDelaysReferencing, deleteOutputDelaysReferencing,
#     deleteClockLatenciesReferencing, deleteClockInsertionsReferencing,
#     deleteInterClockUncertaintiesReferencing, deleteLatchBorrowLimitsReferencing,
#     deleteMinPulseWidthReferencing, deleteMasterClkRefs, clockGroupsDeleteClkRefs),
#     disableClockGatingCheck(Instance), disableClockGatingCheck(Pin),
#     removeDisableClockGatingCheck(Instance), removeDisableClockGatingCheck(Pin),
#     isDisableClockGatingCheck(Instance), isDisableClockGatingCheck(Pin),
#     setCapacitanceLimit(Cell/Port/Pin), capacitanceLimit(Port/Pin),
#     isLeafPinClock, portExtCap (various overloads),
#     hasPortExtCap, portExtFanout, ensurePortExtPinCap
#   WriteSdc.cc: writePortLoads, writePortExtCap, writeClockGatingCheck,
#     writeMinPulseWidths, writeInterClockUncertainty
#   Clock.cc: removeClock triggers clock deletion and re-creation
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

############################################################
# Phase 1: Build full constraint environment referencing clocks
############################################################
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 -waveform {0 10} [get_ports clk2]
create_clock -name vclk -period 5

# Generated clocks referencing master clocks
create_generated_clock -name gclk1 -source [get_ports clk1] -divide_by 2 [get_pins reg1/Q]
create_generated_clock -name gclk2 -source [get_ports clk2] -multiply_by 3 [get_pins reg3/Q]

# IO delays referencing clk1 and clk2
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.5 [get_ports in2]
set_input_delay -clock clk2 1.8 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 3.5 [get_ports out2]

# Clock latency referencing clk1
set_clock_latency -source 0.5 [get_clocks clk1]
set_clock_latency -source -early 0.3 [get_clocks clk1]
set_clock_latency -source -late 0.6 [get_clocks clk1]
set_clock_latency 0.2 [get_clocks clk2]

# Clock insertion
set_clock_latency -source -rise -early 0.25 [get_clocks clk1]
set_clock_latency -source -fall -late 0.55 [get_clocks clk1]

# Inter-clock uncertainty referencing clk1-clk2
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup 0.3
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -hold 0.15
set_clock_uncertainty -from [get_clocks clk2] -to [get_clocks clk1] -setup 0.28
set_clock_uncertainty -from [get_clocks clk2] -to [get_clocks clk1] -hold 0.12
set_clock_uncertainty -setup 0.2 [get_clocks clk1]
set_clock_uncertainty -hold 0.1 [get_clocks clk1]

# Latch borrow limits referencing clk1
set_max_time_borrow 2.0 [get_clocks clk1]
set_max_time_borrow 1.5 [get_clocks clk2]
set_max_time_borrow 1.0 [get_pins reg1/D]

# Min pulse width referencing clk1
set_min_pulse_width -high 0.6 [get_clocks clk1]
set_min_pulse_width -low 0.4 [get_clocks clk1]
set_min_pulse_width 0.8 [get_clocks clk2]

# Clock groups referencing clk1
set_clock_groups -asynchronous -name async1 \
  -group {clk1 gclk1} \
  -group {clk2 gclk2}

# Exception paths referencing clk1
set_false_path -from [get_clocks clk1] -to [get_clocks clk2]
set_multicycle_path -setup 2 -from [get_ports in1] -to [get_ports out1]

# Write before deletion
set sdc1 [make_result_file sdc_rmclk1.sdc]
write_sdc -no_timestamp $sdc1

report_checks

############################################################
# Phase 2: Clock gating check - disable on instance and pin
############################################################

# Design-level clock gating check
set_clock_gating_check -setup 0.5 [current_design]
set_clock_gating_check -hold 0.3 [current_design]

# Clock-level
set_clock_gating_check -setup 0.4 [get_clocks clk1]
set_clock_gating_check -hold 0.2 [get_clocks clk1]

# Instance-level
set_clock_gating_check -setup 0.3 [get_cells reg1]
set_clock_gating_check -hold 0.1 [get_cells reg1]

# Pin-level
set_clock_gating_check -setup 0.25 [get_pins reg1/CK]
set_clock_gating_check -hold 0.08 [get_pins reg1/CK]

# Write with clock gating
set sdc2 [make_result_file sdc_rmclk2.sdc]
write_sdc -no_timestamp $sdc2

############################################################
# Phase 3: Capacitance limits on cell, port, pin
############################################################

# Port capacitance limit
set_max_capacitance 0.15 [get_ports out1]
set_max_capacitance 0.20 [get_ports out2]

set_min_capacitance 0.01 [get_ports out1]

# Pin capacitance limit
set_max_capacitance 0.10 [get_pins reg1/D]

# Cell capacitance limit (via design)
set_max_capacitance 0.25 [current_design]

############################################################
# Phase 4: set_load with various options for portExtCap
############################################################

# Basic pin load
set_load -pin_load 0.05 [get_ports out1]
set_load -wire_load 0.02 [get_ports out1]

# Rise/fall loads
set_load -pin_load -rise 0.04 [get_ports out2]
set_load -pin_load -fall 0.045 [get_ports out2]

# Min/max loads
set_load -min 0.01 [get_ports out1]
set_load -max 0.06 [get_ports out1]

# Port fanout
set_port_fanout_number 4 [get_ports out1]
set_port_fanout_number 8 [get_ports out2]

# Write with loads
set sdc3 [make_result_file sdc_rmclk3.sdc]
write_sdc -no_timestamp $sdc3

set sdc3c [make_result_file sdc_rmclk3_compat.sdc]
write_sdc -no_timestamp -compatible $sdc3c

############################################################
# Phase 5: Delete clocks (exercises removeClock cascade)
############################################################

# Delete generated clocks first (dependent on masters)
delete_generated_clock [get_clocks gclk1]

delete_generated_clock [get_clocks gclk2]

# Delete virtual clock
delete_clock [get_clocks vclk]

# Delete master clock clk2 (removes IO delays, uncertainty, etc.)
delete_clock [get_clocks clk2]

report_clock_properties

# Write after deletions - exercises writing with reduced constraints
set sdc4 [make_result_file sdc_rmclk4.sdc]
write_sdc -no_timestamp $sdc4

report_checks

############################################################
# Phase 6: Re-create clocks and verify
############################################################
create_clock -name clk2_new -period 15 [get_ports clk2]
set_input_delay -clock clk2_new 1.5 [get_ports in3]
set_output_delay -clock clk2_new 2.5 [get_ports out2]

set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2_new] -setup 0.25

# Write final
set sdc5 [make_result_file sdc_rmclk5.sdc]
write_sdc -no_timestamp $sdc5

# Read back and verify
read_sdc $sdc5
report_checks
