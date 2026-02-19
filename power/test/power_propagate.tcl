# Test power propagation, per-instance power breakdown, and activity querying.
# Targets: Power.cc uncovered functions:
#   ensureActivities, propagateActivities, seedActivities,
#   findActivityPeriod, annotateActivities,
#   internalPower, switchingPower, leakagePower per-instance,
#   power for different cell types (sequential, combinational, clock, macro),
#   evalActivity, evalBddDuty, evalBddActivity, evalDiffDuty,
#   findSeqActivity, clockGatePins, inClockNetwork,
#   powerInside, highestPowerInstances, ensureInstPowers

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 1: Power with GCD design and VCD - comprehensive
#---------------------------------------------------------------
puts "--- Test 1: GCD power with VCD ---"
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../examples/gcd_sky130hd.v
link_design gcd
read_sdc ../../examples/gcd_sky130hd.sdc

# Read VCD to seed activities
read_vcd -scope gcd_tb/gcd1 ../../examples/gcd_sky130hd.vcd.gz

# Report power - exercises full power calculation pipeline
report_power

# Report power with different digit counts
report_power -digits 2

report_power -digits 8

#---------------------------------------------------------------
# Test 2: Instance-level power for specific cells
# Exercises: power(Instance) paths for various cell types
#---------------------------------------------------------------
puts "--- Test 2: instance-level power ---"

# Get all register instances
set regs [all_registers -cells]
set reg_count [llength $regs]
puts "register cells: $reg_count"

if { $reg_count > 0 } {
  report_power -instances [lrange $regs 0 2]
}

# Get combinational cells
set all_cells [get_cells *]
set combo_cells [list]
foreach cell $all_cells {
  set cell_ref [get_property $cell ref_name]
  if { [string match "*buf*" $cell_ref] || [string match "*inv*" $cell_ref] || [string match "*nand*" $cell_ref] || [string match "*nor*" $cell_ref] || [string match "*and*" $cell_ref] || [string match "*or*" $cell_ref] } {
    lappend combo_cells $cell
    if { [llength $combo_cells] >= 5 } { break }
  }
}

if { [llength $combo_cells] > 0 } {
  report_power -instances $combo_cells

  report_power -instances $combo_cells -format json

  report_power -instances $combo_cells -digits 6
}

#---------------------------------------------------------------
# Test 3: Highest power instances
# Exercises: highestPowerInstances sorting
#---------------------------------------------------------------
puts "--- Test 3: highest power instances ---"

report_power -highest_power_instances 3

report_power -highest_power_instances 10

report_power -highest_power_instances 5 -format json

report_power -highest_power_instances 3 -digits 6

#---------------------------------------------------------------
# Test 4: Activity annotation report
# Exercises: reportActivityAnnotation paths
#---------------------------------------------------------------
puts "--- Test 4: activity annotation ---"

report_activity_annotation

report_activity_annotation -report_annotated

report_activity_annotation -report_unannotated

#---------------------------------------------------------------
# Test 5: Override VCD activities with manual settings
# Exercises: setGlobalActivity, setInputActivity, setUserActivity paths
#---------------------------------------------------------------
puts "--- Test 5: activity overrides ---"

# Set global activity - overrides VCD
set_power_activity -global -activity 0.5 -duty 0.5
report_power

# Unset global
unset_power_activity -global

# Set input activity
set_power_activity -input -activity 0.3 -duty 0.4
report_power
unset_power_activity -input

# Set per-port activity
set_power_activity -input_ports [get_ports req_msg[0]] -activity 0.8 -duty 0.5
report_power
unset_power_activity -input_ports [get_ports req_msg[0]]

# Set per-pin activity
set clk_pins [get_pins */CLK]
if { [llength $clk_pins] > 0 } {
  set first_clk [lindex $clk_pins 0]
  set_power_activity -pins $first_clk -activity 1.0 -duty 0.5
  report_power
  unset_power_activity -pins $first_clk
}

# Set density-based activity
set_power_activity -global -density 5e8 -duty 0.5
report_power
unset_power_activity -global

#---------------------------------------------------------------
# Test 6: SAIF-based power (different activity source)
# Exercises: readSaif path, SAIF annotation
#---------------------------------------------------------------
puts "--- Test 6: SAIF power ---"
# Re-read design
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../examples/gcd_sky130hd.v
link_design gcd
read_sdc ../../examples/gcd_sky130hd.sdc

read_saif -scope gcd_tb/gcd1 ../../examples/gcd_sky130hd.saif.gz

report_power

report_power -highest_power_instances 5

report_activity_annotation

#---------------------------------------------------------------
# Test 7: Power with no activity data (default propagation)
# Exercises: ensureActivities default input activity paths
#---------------------------------------------------------------
puts "--- Test 7: power with defaults ---"
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../examples/gcd_sky130hd.v
link_design gcd
read_sdc ../../examples/gcd_sky130hd.sdc

# No VCD or SAIF - relies on default activity propagation
report_power

report_power -format json

report_activity_annotation
