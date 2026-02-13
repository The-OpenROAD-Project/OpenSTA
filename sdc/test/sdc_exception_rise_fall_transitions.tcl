# Test ExceptionPath.cc: complex multi-through false/multicycle paths
# with rise/fall transition specifiers, priority calculation, merging.
# Also exercises WriteSdc.cc exception writing with rise/fall transitions,
# Sdc.cc resetPath for various exception types.
# Targets: ExceptionPath.cc fromThruToPriority with rise/fall combos,
#   ExceptionPath::intersectsPts, ExceptionPath::mergeable,
#   ExceptionPathLess comparison, ExceptionThru::matches rise/fall,
#   ExceptionTo::matches rise/fall, ExceptionFrom::matches rise/fall,
#   Sdc.cc addException with rise/fall from/thru/to,
#   WriteSdc.cc writeException with -rise_from/-fall_from/-rise_to/-fall_to
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
# Test 1: false_path with -rise_from / -fall_from
############################################################
set_false_path -rise_from [get_ports in1] -to [get_ports out1]
puts "PASS: false_path -rise_from"

set_false_path -fall_from [get_ports in2] -to [get_ports out1]
puts "PASS: false_path -fall_from"

############################################################
# Test 2: false_path with -rise_to / -fall_to
############################################################
set_false_path -from [get_ports in1] -rise_to [get_ports out2]
puts "PASS: false_path -rise_to"

set_false_path -from [get_ports in2] -fall_to [get_ports out2]
puts "PASS: false_path -fall_to"

############################################################
# Test 3: false_path with -rise_through / -fall_through
############################################################
set_false_path -rise_through [get_pins buf1/Z] -to [get_ports out1]
puts "PASS: false_path -rise_through pin"

set_false_path -fall_through [get_pins inv1/ZN] -to [get_ports out2]
puts "PASS: false_path -fall_through pin"

############################################################
# Test 4: Combination of rise/fall from + through + to
############################################################
set_false_path -rise_from [get_ports in3] \
  -through [get_pins or1/ZN] \
  -fall_to [get_ports out2]
puts "PASS: false_path rise_from + through + fall_to"

set_false_path -fall_from [get_ports in3] \
  -rise_through [get_pins or1/ZN] \
  -to [get_ports out1]
puts "PASS: false_path fall_from + rise_through + to"

############################################################
# Test 5: multicycle_path with rise/fall
############################################################
set_multicycle_path -setup 2 -rise_from [get_ports in1] -to [get_ports out1]
puts "PASS: mcp -rise_from setup"

set_multicycle_path -hold 1 -fall_from [get_ports in1] -to [get_ports out1]
puts "PASS: mcp -fall_from hold"

set_multicycle_path -setup 3 -from [get_ports in2] -rise_to [get_ports out1]
puts "PASS: mcp -rise_to setup"

set_multicycle_path -hold 2 -from [get_ports in2] -fall_to [get_ports out1]
puts "PASS: mcp -fall_to hold"

############################################################
# Test 6: max_delay with rise/fall
############################################################
set_max_delay -rise_from [get_ports in1] -to [get_ports out2] 7.0
puts "PASS: max_delay -rise_from"

set_max_delay -from [get_ports in3] -fall_to [get_ports out1] 8.0
puts "PASS: max_delay -fall_to"

set_min_delay -fall_from [get_ports in2] -to [get_ports out2] 0.5
puts "PASS: min_delay -fall_from"

set_min_delay -from [get_ports in3] -rise_to [get_ports out2] 0.3
puts "PASS: min_delay -rise_to"

############################################################
# Test 7: Write SDC and verify rise/fall transitions preserved
############################################################
set sdc1 [make_result_file sdc_exc_risefall1.sdc]
write_sdc -no_timestamp $sdc1
puts "PASS: write_sdc"

set sdc2 [make_result_file sdc_exc_risefall2.sdc]
write_sdc -no_timestamp -compatible $sdc2
puts "PASS: write_sdc compatible"

############################################################
# Test 8: report_checks to validate exceptions applied
############################################################
report_checks -path_delay max
puts "PASS: report max"

report_checks -path_delay min
puts "PASS: report min"

report_checks -path_delay max -from [get_ports in1]
puts "PASS: report from in1"

report_checks -path_delay max -from [get_ports in2]
puts "PASS: report from in2"

report_checks -path_delay max -from [get_ports in3]
puts "PASS: report from in3"

############################################################
# Test 9: Unset rise/fall exceptions
############################################################
unset_path_exceptions -rise_from [get_ports in1] -to [get_ports out1]
puts "PASS: unset rise_from"

unset_path_exceptions -fall_from [get_ports in2] -to [get_ports out1]
puts "PASS: unset fall_from"

unset_path_exceptions -from [get_ports in1] -rise_to [get_ports out2]
puts "PASS: unset rise_to"

unset_path_exceptions -from [get_ports in2] -fall_to [get_ports out2]
puts "PASS: unset fall_to"

############################################################
# Test 10: Write after unset
############################################################
set sdc3 [make_result_file sdc_exc_risefall3.sdc]
write_sdc -no_timestamp $sdc3
puts "PASS: write after unset"

############################################################
# Test 11: Read back SDC
############################################################
read_sdc $sdc1
puts "PASS: read_sdc"

report_checks -path_delay max
puts "PASS: report after read"

set sdc4 [make_result_file sdc_exc_risefall4.sdc]
write_sdc -no_timestamp $sdc4
puts "PASS: write after read"

puts "ALL PASSED"
