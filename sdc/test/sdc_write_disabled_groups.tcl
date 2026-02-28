# Test write_sdc paths for disabled ports, clock groups, group paths,
# output drives, inter-clock uncertainty, and min pulse width writing.
# Targets:
#   WriteSdc.cc: writeDisabledPorts (disabled top-level ports),
#     writeDisabledInstances (disabled instances with from/to),
#     writeFalsePaths (false_path writing with setup/hold),
#     writeExceptionThruPins/Nets/Instances,
#     writeGroupPath (named and default, with weight being ignored),
#     writeOutputDrives (set_driving_cell, set_drive),
#     writeMinPulseWidths (on design/clock/pin/instance),
#     writePortExtCap (pin/wire/fanout writing),
#     writeInterClockUncertainty (all rise/fall/setup/hold combos),
#     writeClockGroups (async/logically_excl/physically_excl),
#     writePropagatedClkPins, writeClockInsertions,
#     writeClockSenses
#   Sdc.cc: various getter functions exercised during write
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
create_clock -name vclk -period 8
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_input_delay -clock clk2 2.0 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]

############################################################
# Disable timing on ports (exercises writeDisabledPorts)
############################################################
set_disable_timing [get_ports in1]
set_disable_timing [get_ports in2]

# Disable timing on instances with from/to (exercises writeDisabledInstances)
set_disable_timing [get_cells buf1]
set_disable_timing [get_cells inv1]

# Disable on instance with from/to pins
set_disable_timing -from A -to ZN [get_cells inv1]

# Disable on lib cell (exercises writeDisabledCells)
set_disable_timing [get_lib_cells NangateOpenCellLibrary/AND2_X1] -from A1 -to ZN
set_disable_timing [get_lib_cells NangateOpenCellLibrary/OR2_X1]

# Disable on pins
set_disable_timing [get_pins nand1/A1]

# Write with disables
set sdc1 [make_result_file sdc_wdg1.sdc]
write_sdc -no_timestamp $sdc1

# Unset disables
unset_disable_timing [get_ports in1]
unset_disable_timing [get_ports in2]
unset_disable_timing [get_cells buf1]
unset_disable_timing [get_cells inv1]
unset_disable_timing [get_lib_cells NangateOpenCellLibrary/AND2_X1] -from A1 -to ZN
unset_disable_timing [get_lib_cells NangateOpenCellLibrary/OR2_X1]
unset_disable_timing [get_pins nand1/A1]

############################################################
# Clock groups - all three types (exercises writeClockGroups)
############################################################
set_clock_groups -asynchronous -name async1 -group {clk1} -group {clk2}

set sdc2 [make_result_file sdc_wdg2.sdc]
write_sdc -no_timestamp $sdc2

unset_clock_groups -asynchronous -name async1

set_clock_groups -logically_exclusive -name logic1 -group {clk1} -group {clk2}

set sdc3 [make_result_file sdc_wdg3.sdc]
write_sdc -no_timestamp $sdc3

unset_clock_groups -logically_exclusive -name logic1

set_clock_groups -physically_exclusive -name phys1 -group {clk1} -group {vclk}

set sdc4 [make_result_file sdc_wdg4.sdc]
write_sdc -no_timestamp $sdc4

unset_clock_groups -physically_exclusive -name phys1

############################################################
# Group paths - named and default (exercises writeGroupPath)
############################################################
group_path -name grp_reg -from [get_clocks clk1] -to [get_clocks clk1]
group_path -name grp_cross -from [get_clocks clk1] -to [get_clocks clk2]
group_path -default -from [get_ports in1] -to [get_ports out1]

# Group path with weight (weight is ignored but syntax is accepted)
group_path -name grp_weighted -weight 2.0 \
  -from [get_ports in2] -to [get_ports out2]

# Group path with through
group_path -name grp_thru -from [get_ports in1] \
  -through [get_pins buf1/Z] \
  -through [get_pins and1/ZN] \
  -to [get_ports out1]

############################################################
# Output drives (exercises writeOutputDrives/writeDriveResistances)
############################################################
set_driving_cell -lib_cell BUF_X1 [get_ports in1]
set_driving_cell -lib_cell INV_X1 -pin ZN [get_ports in2]
set_driving_cell -lib_cell BUF_X4 [get_ports in3]

set_drive 100 [get_ports in1]
set_drive -rise 80 [get_ports in2]
set_drive -fall 120 [get_ports in2]

# Input transition
set_input_transition 0.15 [get_ports in1]
set_input_transition -rise -max 0.12 [get_ports in2]
set_input_transition -fall -min 0.08 [get_ports in2]

############################################################
# Inter-clock uncertainty with all combinations
# (exercises writeInterClockUncertainty)
############################################################
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup 0.3
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -hold 0.15
set_clock_uncertainty -from [get_clocks clk2] -to [get_clocks clk1] -setup 0.28
set_clock_uncertainty -from [get_clocks clk2] -to [get_clocks clk1] -hold 0.12

############################################################
# Min pulse width on multiple target types
# (exercises writeMinPulseWidths)
############################################################
set_min_pulse_width 0.5

set_min_pulse_width -high 0.6 [get_clocks clk1]
set_min_pulse_width -low 0.4 [get_clocks clk1]

set_min_pulse_width 0.55 [get_clocks clk2]

set_min_pulse_width 0.3 [get_pins reg1/CK]

set_min_pulse_width 0.45 [get_cells reg3]

############################################################
# Port loads (exercises writePortLoads/writePortExtCap)
############################################################
set_load -pin_load 0.05 [get_ports out1]
set_load -wire_load 0.02 [get_ports out1]
set_load -pin_load -rise 0.04 [get_ports out2]
set_load -pin_load -fall 0.045 [get_ports out2]
set_port_fanout_number 4 [get_ports out1]

############################################################
# Clock sense (exercises writeClockSenses)
############################################################
set_clock_sense -positive -clocks [get_clocks clk1] [get_pins buf1/Z]
set_clock_sense -negative -clocks [get_clocks clk2] [get_pins or1/ZN]

############################################################
# Propagated clocks (exercises writePropagatedClkPins)
############################################################
set_propagated_clock [get_clocks clk1]
set_propagated_clock [get_ports clk2]

# Clock insertion (exercises writeClockInsertions)
set_clock_latency -source -early 0.3 [get_clocks clk1]
set_clock_latency -source -late 0.5 [get_clocks clk1]

############################################################
# Clock transition
############################################################
set_clock_transition -rise -max 0.15 [get_clocks clk1]
set_clock_transition -fall -min 0.08 [get_clocks clk1]
set_clock_transition 0.1 [get_clocks clk2]

############################################################
# False paths with -setup/-hold only
# (exercises writeFalsePaths branches)
############################################################
set_false_path -setup -from [get_clocks clk1] -to [get_clocks clk2]
set_false_path -hold -from [get_clocks clk2] -to [get_clocks clk1]

############################################################
# Comprehensive write with all constraint types
############################################################
set sdc5 [make_result_file sdc_wdg5.sdc]
write_sdc -no_timestamp $sdc5

set sdc6 [make_result_file sdc_wdg6.sdc]
write_sdc -no_timestamp -compatible $sdc6

set sdc7 [make_result_file sdc_wdg7.sdc]
write_sdc -no_timestamp -digits 8 $sdc7

set sdc8 [make_result_file sdc_wdg8.sdc]
write_sdc -no_timestamp -map_hpins $sdc8

report_checks

############################################################
# Read back SDC and verify roundtrip
############################################################
read_sdc $sdc5

set sdc9 [make_result_file sdc_wdg9.sdc]
write_sdc -no_timestamp $sdc9

report_checks
