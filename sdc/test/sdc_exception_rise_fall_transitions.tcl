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

############################################################
# Test 1: false_path with -rise_from / -fall_from
############################################################
set_false_path -rise_from [get_ports in1] -to [get_ports out1]

set_false_path -fall_from [get_ports in2] -to [get_ports out1]

############################################################
# Test 2: false_path with -rise_to / -fall_to
############################################################
set_false_path -from [get_ports in1] -rise_to [get_ports out2]

set_false_path -from [get_ports in2] -fall_to [get_ports out2]

############################################################
# Test 3: false_path with -rise_through / -fall_through
############################################################
set_false_path -rise_through [get_pins buf1/Z] -to [get_ports out1]

set_false_path -fall_through [get_pins inv1/ZN] -to [get_ports out2]

############################################################
# Test 4: Combination of rise/fall from + through + to
############################################################
set_false_path -rise_from [get_ports in3] \
  -through [get_pins or1/ZN] \
  -fall_to [get_ports out2]

set_false_path -fall_from [get_ports in3] \
  -rise_through [get_pins or1/ZN] \
  -to [get_ports out1]

############################################################
# Test 5: multicycle_path with rise/fall
############################################################
set_multicycle_path -setup 2 -rise_from [get_ports in1] -to [get_ports out1]

set_multicycle_path -hold 1 -fall_from [get_ports in1] -to [get_ports out1]

set_multicycle_path -setup 3 -from [get_ports in2] -rise_to [get_ports out1]

set_multicycle_path -hold 2 -from [get_ports in2] -fall_to [get_ports out1]

############################################################
# Test 6: max_delay with rise/fall
############################################################
set_max_delay -rise_from [get_ports in1] -to [get_ports out2] 7.0

set_max_delay -from [get_ports in3] -fall_to [get_ports out1] 8.0

set_min_delay -fall_from [get_ports in2] -to [get_ports out2] 0.5

set_min_delay -from [get_ports in3] -rise_to [get_ports out2] 0.3

############################################################
# Test 7: Write SDC and verify rise/fall transitions preserved
############################################################
set sdc1 [make_result_file sdc_exc_risefall1.sdc]
write_sdc -no_timestamp $sdc1

set sdc2 [make_result_file sdc_exc_risefall2.sdc]
write_sdc -no_timestamp -compatible $sdc2

############################################################
# Test 8: report_checks to validate exceptions applied
############################################################
report_checks -path_delay max

report_checks -path_delay min

report_checks -path_delay max -from [get_ports in1]

report_checks -path_delay max -from [get_ports in2]

report_checks -path_delay max -from [get_ports in3]

############################################################
# Test 9: Unset rise/fall exceptions
############################################################
unset_path_exceptions -rise_from [get_ports in1] -to [get_ports out1]

unset_path_exceptions -fall_from [get_ports in2] -to [get_ports out1]

unset_path_exceptions -from [get_ports in1] -rise_to [get_ports out2]

unset_path_exceptions -from [get_ports in2] -fall_to [get_ports out2]

############################################################
# Test 10: Write after unset
############################################################
set sdc3 [make_result_file sdc_exc_risefall3.sdc]
write_sdc -no_timestamp $sdc3

############################################################
# Test 11: Read back SDC
############################################################
read_sdc $sdc1

report_checks -path_delay max

set sdc4 [make_result_file sdc_exc_risefall4.sdc]
write_sdc -no_timestamp $sdc4
