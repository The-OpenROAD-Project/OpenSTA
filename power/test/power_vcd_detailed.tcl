# Test VCD-based power analysis with detailed reporting and instance breakdown
# Targets: Power.cc (power calculation with VCD activity, highestPowerInstances,
#   instance power with various options, power with different activity sources)
#   VcdReader.cc (VCD parsing, activity annotation, bus handling in VCD)
#   VcdParse.cc (VCD file parsing, timestamp handling, value change recording)
#   SaifReader.cc (SAIF reading and activity annotation)

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 1: VCD-based power with full reporting
#---------------------------------------------------------------
puts "--- Test 1: VCD power full report ---"
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../examples/gcd_sky130hd.v
link_design gcd
read_sdc ../../examples/gcd_sky130hd.sdc

# Read VCD
read_vcd -scope gcd_tb/gcd1 ../../examples/gcd_sky130hd.vcd.gz

# Activity annotation reports
report_activity_annotation

report_activity_annotation -report_annotated

report_activity_annotation -report_unannotated

#---------------------------------------------------------------
# Test 2: Power report with various digits
#---------------------------------------------------------------
puts "--- Test 2: power with digits ---"
report_power

report_power -digits 3

report_power -digits 6

report_power -digits 8

#---------------------------------------------------------------
# Test 3: JSON format power
#---------------------------------------------------------------
puts "--- Test 3: JSON power ---"
report_power -format json

report_power -format json -digits 4

#---------------------------------------------------------------
# Test 4: Highest power instances
#---------------------------------------------------------------
puts "--- Test 4: highest power instances ---"
report_power -highest_power_instances 3

report_power -highest_power_instances 5

report_power -highest_power_instances 10

report_power -highest_power_instances 3 -format json

report_power -highest_power_instances 5 -digits 4

report_power -highest_power_instances 5 -format json -digits 6

#---------------------------------------------------------------
# Test 5: Instance power with VCD
#---------------------------------------------------------------
puts "--- Test 5: instance power ---"
# Get some cells and report their power
set some_cells [get_cells _*_]
set cell_count [llength $some_cells]
puts "cells for instance power: $cell_count"

set rc [catch {
  report_power -instances $some_cells
} msg]
if { $rc == 0 } {
} else {
  puts "INFO: report_power -instances: $msg"
}

set rc [catch {
  report_power -instances $some_cells -format json
} msg]
if { $rc == 0 } {
} else {
  puts "INFO: report_power -instances json: $msg"
}

set rc [catch {
  report_power -instances $some_cells -digits 6
} msg]
if { $rc == 0 } {
} else {
  puts "INFO: report_power -instances -digits 6: $msg"
}

#---------------------------------------------------------------
# Test 6: Override VCD with manual activity
#---------------------------------------------------------------
puts "--- Test 6: manual activity override ---"
set_power_activity -global -activity 0.5 -duty 0.5
report_power

unset_power_activity -global
report_power

set_power_activity -input -activity 0.3 -duty 0.4
report_power

unset_power_activity -input
report_power

#---------------------------------------------------------------
# Test 7: Pin-level activity with VCD
#---------------------------------------------------------------
puts "--- Test 7: pin-level activity ---"
set clk_pin [get_pins */_clk_/CLK]
if { [llength $clk_pin] > 0 } {
  set_power_activity -pins $clk_pin -activity 1.0 -duty 0.5
  report_power
  unset_power_activity -pins $clk_pin
}

#---------------------------------------------------------------
# Test 8: Density-based activity with VCD
#---------------------------------------------------------------
puts "--- Test 8: density activity ---"
set_power_activity -global -density 1e8 -duty 0.5
report_power

report_power -format json

unset_power_activity -global

#---------------------------------------------------------------
# Test 9: SAIF-based power (compare with VCD)
#---------------------------------------------------------------
puts "--- Test 9: SAIF power ---"
# Re-read design fresh
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../examples/gcd_sky130hd.v
link_design gcd
read_sdc ../../examples/gcd_sky130hd.sdc

# Read SAIF
read_saif -scope gcd_tb/gcd1 ../../examples/gcd_sky130hd.saif.gz

report_activity_annotation

report_power

report_power -highest_power_instances 5

report_power -format json

report_power -digits 5
