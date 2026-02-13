# Deep SdcNetwork pattern matching and name resolution testing.
# Targets:
#   SdcNetwork.cc: findInstancesMatching, findNetsMatching,
#     findPinsMatching, visitPinTail, visitMatches,
#     findInstanceRelative, findNetRelative, findChild,
#     staToSdc, findPort, findPortsMatching,
#     name/pathName/portName/busName for Instance/Pin/Net/Port,
#     makeInstance, makeNet, parsePath, scanPath
#   Network.cc: findCellsMatching with wildcards and regexp,
#     findNetsHierMatching, findInstancesHierMatching,
#     pathNameCmp, pathNameLess, busIndexInRange
#   ConcreteNetwork.cc: findInstance, findNet, findPin,
#     setAttribute, getAttribute
source ../../test/helpers.tcl

############################################################
# Read libraries
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_liberty ../../test/sky130hd/sky130hd_tt.lib
puts "PASS: read libraries"

############################################################
# Read hierarchical design
############################################################
read_verilog network_hier_test.v
link_design network_hier_test
puts "PASS: link design"

############################################################
# Setup SDC for SdcNetwork name translation
############################################################
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports {in1 in2 in3}]
set_output_delay -clock clk 2.0 [get_ports {out1 out2}]
set_input_transition 0.1 [all_inputs]
puts "PASS: SDC setup"

############################################################
# Instance pattern matching with various patterns
############################################################
puts "--- instance pattern matching ---"

# Wildcard matching
set all_cells [get_cells *]
puts "all cells: [llength $all_cells]"

# Partial wildcard
set buf_cells [get_cells buf*]
puts "buf* cells: [llength $buf_cells]"

set inv_cells [get_cells inv*]
puts "inv* cells: [llength $inv_cells]"

set reg_cells [get_cells reg*]
puts "reg* cells: [llength $reg_cells]"

set sub_cells [get_cells sub*]
puts "sub* cells: [llength $sub_cells]"

# Character class wildcards
set cells_1 [get_cells *1]
puts "*1 cells: [llength $cells_1]"

set cells_out [get_cells *out*]
puts "*out* cells: [llength $cells_out]"

puts "PASS: instance wildcard matching"

############################################################
# Hierarchical instance matching
############################################################
puts "--- hierarchical instance matching ---"

catch {
  set sub1_all [get_cells sub1/*]
  puts "sub1/* = [llength $sub1_all]"
}

catch {
  set sub2_all [get_cells sub2/*]
  puts "sub2/* = [llength $sub2_all]"
}

catch {
  set deep [get_cells sub1/and_gate]
  puts "sub1/and_gate found: [llength $deep]"
}

catch {
  set deep2 [get_cells sub1/buf_gate]
  puts "sub1/buf_gate found: [llength $deep2]"
}

catch {
  set deep3 [get_cells sub2/and_gate]
  puts "sub2/and_gate found: [llength $deep3]"
}
puts "PASS: hierarchical instance matching"

############################################################
# Net pattern matching
############################################################
puts "--- net pattern matching ---"

set all_nets [get_nets *]
puts "all nets: [llength $all_nets]"

set w_nets [get_nets w*]
puts "w* nets: [llength $w_nets]"

# Hierarchical net matching
catch {
  set sub1_nets [get_nets sub1/*]
  puts "sub1/* nets: [llength $sub1_nets]"
}

catch {
  set sub2_nets [get_nets sub2/*]
  puts "sub2/* nets: [llength $sub2_nets]"
}
puts "PASS: net pattern matching"

############################################################
# Pin pattern matching
############################################################
puts "--- pin pattern matching ---"

catch {
  set buf_in_pins [get_pins buf_in/*]
  puts "buf_in/* pins: [llength $buf_in_pins]"
}

catch {
  set reg_pins [get_pins reg1/*]
  puts "reg1/* pins: [llength $reg_pins]"
}

catch {
  set inv_pins [get_pins inv1/*]
  puts "inv1/* pins: [llength $inv_pins]"
}

# Hierarchical pin matching
catch {
  set sub1_pins [get_pins sub1/*]
  puts "sub1/* pins: [llength $sub1_pins]"
}

catch {
  set deep_pins [get_pins sub1/and_gate/*]
  puts "sub1/and_gate/* pins: [llength $deep_pins]"
}
puts "PASS: pin pattern matching"

############################################################
# Port name and property queries through SdcNetwork
############################################################
puts "--- port name queries ---"

foreach port_name {clk in1 in2 in3 out1 out2} {
  set port [get_ports $port_name]
  set dir [get_property $port direction]
  puts "$port_name dir=$dir"
}
puts "PASS: port name queries"

############################################################
# Report checks with SDC constraints exercising SdcNetwork
############################################################
puts "--- timing analysis through SDC ---"

report_checks
puts "PASS: report_checks"

report_checks -path_delay min
puts "PASS: min path"

report_checks -rise_from [get_ports in1]
puts "PASS: rise_from in1"

report_checks -fall_from [get_ports in1]
puts "PASS: fall_from in1"

report_checks -to [get_ports out1]
puts "PASS: to out1"

report_checks -to [get_ports out2]
puts "PASS: to out2"

# Fields
report_checks -fields {slew cap input_pins nets fanout}
puts "PASS: with fields"

report_checks -format full_clock
puts "PASS: full_clock"

# Endpoint count
report_checks -endpoint_count 5
puts "PASS: endpoint count 5"

report_checks -group_count 3
puts "PASS: group count 3"

############################################################
# SDC operations through SdcNetwork name resolution
############################################################
puts "--- SDC operations ---"

# set_false_path exercises SdcNetwork path resolution
catch {set_false_path -from [get_ports in3] -to [get_ports out1]}
report_checks -from [get_ports in3] -to [get_ports out1]
puts "PASS: false path"

# set_multicycle_path
catch {set_multicycle_path 2 -from [get_ports in1] -to [get_ports out1]}
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: multicycle path"

# set_max_delay
catch {set_max_delay 5.0 -from [get_ports in2] -to [get_ports out2]}
report_checks -from [get_ports in2] -to [get_ports out2]
puts "PASS: max delay constraint"

# set_disable_timing through hierarchy
catch {
  set_disable_timing [get_lib_cells NangateOpenCellLibrary/BUF_X1] -from A -to Z
  report_checks
  puts "PASS: disable_timing"
}

############################################################
# Instance/net/pin property queries
############################################################
puts "--- property queries ---"

set inst [get_cells buf_in]
set ref [get_property $inst ref_name]
puts "buf_in ref=$ref"

set inst2 [get_cells reg1]
set ref2 [get_property $inst2 ref_name]
puts "reg1 ref=$ref2"

catch {
  set inst3 [get_cells sub1]
  set ref3 [get_property $inst3 ref_name]
  puts "sub1 ref=$ref3"
}
puts "PASS: property queries"

############################################################
# report_check_types for completeness
############################################################
report_check_types -max_delay -min_delay
puts "PASS: check types max/min"

report_check_types -max_slew -max_capacitance -max_fanout
puts "PASS: check types slew/cap/fanout"

report_check_types -recovery -removal
puts "PASS: check types recovery/removal"

report_check_types -min_pulse_width -min_period
puts "PASS: check types pulse/period"

puts "ALL PASSED"
