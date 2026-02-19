# Test GraphDelayCalc incremental delay tolerance and various invalidation paths.
# Targets:
#   GraphDelayCalc.cc: setIncrementalDelayTolerance, incrementalDelayTolerance,
#     seedInvalidDelays, delayInvalid(pin), delayInvalid(vertex),
#     findDelays incremental paths, levelChangedBefore, levelsChangedBefore,
#     deleteVertexBefore, findVertexDelay, findCheckEdgeDelays,
#     findLatchEdgeDelays, netCaps, loadDelay, seedRootSlews
#   DmpCeff.cc: incremental recalculation with tolerance changes
#   NetCaps.cc: net capacitance recomputation after changes
#   ArcDelayCalc.cc: arc delay recalculation paths

source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog dcalc_multidriver_test.v
link_design dcalc_multidriver_test

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {in1 in2 in3 in4 sel}]
set_output_delay -clock clk 0 [get_ports {out1 out2 out3}]
set_input_transition 0.1 [get_ports {in1 in2 in3 in4 sel clk}]

#---------------------------------------------------------------
# Test 1: Baseline timing
#---------------------------------------------------------------
puts "--- Test 1: baseline timing ---"
report_checks

report_checks -path_delay min

#---------------------------------------------------------------
# Test 2: Set incremental delay tolerance
# Exercises: setIncrementalDelayTolerance, incrementalDelayTolerance
#---------------------------------------------------------------
puts "--- Test 2: incremental delay tolerance ---"

# Set large tolerance (will suppress many incremental updates)
sta::set_delay_calc_incremental_tolerance 0.5

# Change input transition - large tolerance means less recalc
set_input_transition 0.2 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]

# Change back
set_input_transition 0.1 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]

# Set small tolerance (will recompute more aggressively)
sta::set_delay_calc_incremental_tolerance 0.001

set_input_transition 0.2 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]

set_input_transition 0.1 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]

# Zero tolerance
sta::set_delay_calc_incremental_tolerance 0.0

#---------------------------------------------------------------
# Test 3: Incremental updates with load changes
# Exercises: seedInvalidDelays, delayInvalid, net cap recomputation
#---------------------------------------------------------------
puts "--- Test 3: incremental load changes ---"

# Successively larger loads on out1
foreach load_val {0.0001 0.001 0.005 0.01 0.05 0.1 0.5} {
  set_load $load_val [get_ports out1]
  report_checks -from [get_ports in1] -to [get_ports out1]
  puts "load=$load_val: done"
}
set_load 0 [get_ports out1]

# Loads on multiple outputs simultaneously
set_load 0.01 [get_ports out1]
set_load 0.01 [get_ports out2]
set_load 0.01 [get_ports out3]
report_checks

set_load 0 [get_ports out1]
set_load 0 [get_ports out2]
set_load 0 [get_ports out3]

#---------------------------------------------------------------
# Test 4: Incremental updates with transition changes
# Exercises: seedRootSlews with varying slew values
#---------------------------------------------------------------
puts "--- Test 4: incremental slew changes ---"

# Very fast transitions
set_input_transition 0.001 [get_ports {in1 in2 in3 in4 sel}]
report_checks

# Medium transitions
set_input_transition 0.1 [get_ports {in1 in2 in3 in4 sel}]
report_checks

# Very slow transitions
set_input_transition 2.0 [get_ports {in1 in2 in3 in4 sel}]
report_checks

# Different slews on different inputs
set_input_transition 0.01 [get_ports in1]
set_input_transition 0.5 [get_ports in2]
set_input_transition 0.001 [get_ports in3]
set_input_transition 1.0 [get_ports in4]
set_input_transition 0.1 [get_ports sel]
report_checks

# Restore
set_input_transition 0.1 [get_ports {in1 in2 in3 in4 sel clk}]

#---------------------------------------------------------------
# Test 5: Incremental timing constraint changes
# Exercises: findDelays with constraint invalidation
#---------------------------------------------------------------
puts "--- Test 5: constraint changes ---"

# Change clock period
create_clock -name clk -period 5 [get_ports clk]
report_checks

create_clock -name clk -period 20 [get_ports clk]
report_checks

create_clock -name clk -period 10 [get_ports clk]

# Change input delays
set_input_delay -clock clk 2.0 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]

set_input_delay -clock clk 0.0 [get_ports in1]

# Change output delays
set_output_delay -clock clk 3.0 [get_ports out1]
report_checks -to [get_ports out1]

set_output_delay -clock clk 0.0 [get_ports out1]

#---------------------------------------------------------------
# Test 6: Network modification triggers delay invalidation
# Exercises: makeInstance, connectPin, disconnectPin => delayInvalid
#---------------------------------------------------------------
puts "--- Test 6: network modification invalidation ---"

# Add new instance
set new_inst [make_instance extra_buf NangateOpenCellLibrary/BUF_X4]

set new_net [make_net extra_net]

connect_pin extra_net extra_buf/A

report_checks

# Disconnect and delete
disconnect_pin extra_net extra_buf/A

delete_instance extra_buf
delete_net extra_net

report_checks

#---------------------------------------------------------------
# Test 7: Replace cell triggers delay recalc
# Exercises: replaceCell => incremental delay update
#---------------------------------------------------------------
puts "--- Test 7: replace cell ---"

# Replace buf1 with larger buffer
replace_cell buf1 NangateOpenCellLibrary/BUF_X4
report_checks -from [get_ports in1] -to [get_ports out1]

replace_cell buf1 NangateOpenCellLibrary/BUF_X2
report_checks -from [get_ports in1] -to [get_ports out1]

replace_cell buf1 NangateOpenCellLibrary/BUF_X1
report_checks -from [get_ports in1] -to [get_ports out1]

# Replace multiple cells
replace_cell and1 NangateOpenCellLibrary/AND2_X2
replace_cell or1 NangateOpenCellLibrary/OR2_X2
report_checks

replace_cell and1 NangateOpenCellLibrary/AND2_X1
replace_cell or1 NangateOpenCellLibrary/OR2_X1
report_checks

#---------------------------------------------------------------
# Test 8: Tolerance with calculator switching
# Exercises: setIncrementalDelayTolerance persists across calc changes
#---------------------------------------------------------------
puts "--- Test 8: tolerance with calculator switching ---"

sta::set_delay_calc_incremental_tolerance 0.1

set_delay_calculator lumped_cap
report_checks

set_delay_calculator dmp_ceff_elmore
report_checks

set_delay_calculator dmp_ceff_two_pole
report_checks

set_delay_calculator unit
report_checks

# Restore
set_delay_calculator dmp_ceff_elmore
sta::set_delay_calc_incremental_tolerance 0.0

report_checks
