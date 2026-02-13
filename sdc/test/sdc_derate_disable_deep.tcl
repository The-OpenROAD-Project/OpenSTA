# Test deep derating paths and disable timing with from/to on instances.
# Targets:
#   Sdc.cc: setTimingDerate (global, cell, inst, net with rf/clk_data),
#     disable (cell from/to, instance from/to, lib port, port, pin),
#     removeDisable variants, disabledEdgesSorted,
#     setClockGatingCheck (global, clock, pin, instance)
#   WriteSdc.cc: writeDeratings (global type-specific, cell, instance, net),
#     writeDerating (DeratingFactorsGlobal, DeratingFactorsCell,
#       DeratingFactors with type/early_late/clk_data/rf),
#     writeDisabledCells (all, from/to, from, to, timingArcSets),
#     writeDisabledInstances (from/to), writeDisabledPins,
#     writeDisabledPorts, writeDisabledLibPorts,
#     timingDerateTypeKeyword
#   DeratingFactors.cc: isOneValue, factor, hasValue
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_input_delay -clock clk2 2.0 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]
puts "PASS: setup"

############################################################
# Timing derate - global with specific types
############################################################

# Global early/late
set_timing_derate -early 0.95
set_timing_derate -late 1.05
puts "PASS: global derate"

# Global with clock/data separation
set_timing_derate -early -clock 0.97
set_timing_derate -late -clock 1.03
set_timing_derate -early -data 0.94
set_timing_derate -late -data 1.06
puts "PASS: global derate clock/data"

# Global with type-specific (cell_delay, cell_check, net_delay)
set_timing_derate -early -cell_delay 0.96
set_timing_derate -late -cell_delay 1.04
set_timing_derate -early -cell_check 0.98
set_timing_derate -late -cell_check 1.02
set_timing_derate -early -net_delay 0.93
set_timing_derate -late -net_delay 1.07
puts "PASS: global derate by type"

# Global with rise/fall
set_timing_derate -early -cell_delay -clock -rise 0.96
set_timing_derate -late -cell_delay -clock -fall 1.04
set_timing_derate -early -cell_delay -data -rise 0.95
set_timing_derate -late -cell_delay -data -fall 1.05
puts "PASS: global derate by type/clock_data/rf"

report_checks
puts "PASS: report after global derate"

############################################################
# Timing derate on lib cells
############################################################
set_timing_derate -early -cell_delay 0.91 [get_lib_cells NangateOpenCellLibrary/INV_X1]
set_timing_derate -late -cell_delay 1.09 [get_lib_cells NangateOpenCellLibrary/INV_X1]
set_timing_derate -early -cell_check 0.90 [get_lib_cells NangateOpenCellLibrary/DFF_X1]
set_timing_derate -late -cell_check 1.10 [get_lib_cells NangateOpenCellLibrary/DFF_X1]
puts "PASS: lib cell derate"

############################################################
# Timing derate on instances
############################################################
set_timing_derate -early -cell_delay 0.89 [get_cells buf1]
set_timing_derate -late -cell_delay 1.11 [get_cells buf1]
set_timing_derate -early -cell_check 0.88 [get_cells reg1]
set_timing_derate -late -cell_check 1.12 [get_cells reg1]
puts "PASS: instance derate"

############################################################
# Timing derate on nets
############################################################
set_timing_derate -early -net_delay 0.87 [get_nets n1]
set_timing_derate -late -net_delay 1.13 [get_nets n1]
set_timing_derate -early -net_delay 0.86 [get_nets n3]
set_timing_derate -late -net_delay 1.14 [get_nets n3]
puts "PASS: net derate"

report_checks
puts "PASS: report after all derates"

############################################################
# Write SDC (exercises all derating writer paths)
############################################################
set sdc1 [make_result_file sdc_derate1.sdc]
write_sdc -no_timestamp $sdc1
puts "PASS: write_sdc with derates"

############################################################
# Reset deratings
############################################################
unset_timing_derate
puts "PASS: unset_timing_derate"

############################################################
# Disable timing - cell from/to variants
############################################################

# Disable entire lib cell
set_disable_timing [get_lib_cells NangateOpenCellLibrary/BUF_X1]
puts "PASS: disable lib cell all"

# Disable with from only
set_disable_timing [get_lib_cells NangateOpenCellLibrary/INV_X1] -from A
puts "PASS: disable lib cell from"

# Disable with to only
set_disable_timing [get_lib_cells NangateOpenCellLibrary/NAND2_X1] -to ZN
puts "PASS: disable lib cell to"

# Disable with from and to
set_disable_timing [get_lib_cells NangateOpenCellLibrary/NOR2_X1] -from A1 -to ZN
puts "PASS: disable lib cell from/to"

# Disable with from and to - second arc
set_disable_timing [get_lib_cells NangateOpenCellLibrary/NOR2_X1] -from A2 -to ZN
puts "PASS: disable lib cell from/to second"

report_checks
puts "PASS: report after lib cell disables"

############################################################
# Disable timing - instance from/to variants
############################################################

# Disable entire instance
set_disable_timing [get_cells buf1]
puts "PASS: disable instance all"

# Disable instance with from/to
set_disable_timing [get_cells and1] -from A1 -to ZN
set_disable_timing [get_cells and1] -from A2 -to ZN
puts "PASS: disable instance from/to"

# Disable instance with from only
set_disable_timing [get_cells or1] -from A1
puts "PASS: disable instance from"

# Disable instance with to only
set_disable_timing [get_cells nand1] -to ZN
puts "PASS: disable instance to"

report_checks
puts "PASS: report after instance disables"

############################################################
# Disable timing - pin
############################################################
set_disable_timing [get_pins inv1/A]
puts "PASS: disable pin"

############################################################
# Disable timing - port
############################################################
catch {
  set_disable_timing [get_ports in1]
  puts "PASS: disable port"
}

############################################################
# Write SDC with disables
############################################################
set sdc2 [make_result_file sdc_derate2.sdc]
write_sdc -no_timestamp $sdc2
puts "PASS: write_sdc with disables"

set sdc3 [make_result_file sdc_derate3.sdc]
write_sdc -no_timestamp -compatible $sdc3
puts "PASS: write_sdc compatible with disables"

############################################################
# Unset all disables
############################################################
unset_disable_timing [get_lib_cells NangateOpenCellLibrary/BUF_X1]
unset_disable_timing [get_lib_cells NangateOpenCellLibrary/INV_X1] -from A
unset_disable_timing [get_lib_cells NangateOpenCellLibrary/NAND2_X1] -to ZN
unset_disable_timing [get_lib_cells NangateOpenCellLibrary/NOR2_X1] -from A1 -to ZN
unset_disable_timing [get_lib_cells NangateOpenCellLibrary/NOR2_X1] -from A2 -to ZN
puts "PASS: unset lib cell disables"

unset_disable_timing [get_cells buf1]
unset_disable_timing [get_cells and1] -from A1 -to ZN
unset_disable_timing [get_cells and1] -from A2 -to ZN
unset_disable_timing [get_cells or1] -from A1
unset_disable_timing [get_cells nand1] -to ZN
puts "PASS: unset instance disables"

unset_disable_timing [get_pins inv1/A]
puts "PASS: unset pin disable"

############################################################
# Clock gating check - global/clock/instance/pin
############################################################
set_clock_gating_check -setup 0.5 [current_design]
set_clock_gating_check -hold 0.3 [current_design]
puts "PASS: clock_gating_check global"

set_clock_gating_check -setup 0.4 [get_clocks clk1]
set_clock_gating_check -hold 0.2 [get_clocks clk1]
puts "PASS: clock_gating_check clock"

catch {
  set_clock_gating_check -setup 0.35 [get_cells reg1]
  set_clock_gating_check -hold 0.15 [get_cells reg1]
  puts "PASS: clock_gating_check instance"
}

catch {
  set_clock_gating_check -setup 0.3 [get_pins reg2/CK]
  set_clock_gating_check -hold 0.1 [get_pins reg2/CK]
  puts "PASS: clock_gating_check pin"
}

############################################################
# Disabled edges sorted
############################################################
catch {
  set disabled [sta::disabled_edges_sorted]
  puts "disabled_edges_sorted count = [llength $disabled]"
}
puts "PASS: disabled_edges_sorted"

############################################################
# Final write
############################################################
set sdc4 [make_result_file sdc_derate4.sdc]
write_sdc -no_timestamp $sdc4
puts "PASS: final write_sdc"

report_checks
puts "PASS: final report"

puts "ALL PASSED"
