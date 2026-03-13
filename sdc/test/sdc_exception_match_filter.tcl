# Test ExceptionPath.cc matching and filtering: exception path state matching,
# multi-thru point matching, matchesFilter, and exception path deletion.
# Targets: ExceptionPath.cc ExceptionState matching,
#   ExceptionTo::matchesFilter, ExceptionThru::allPins,
#   ExceptionPath::tighterThan, ExceptionPath::overrides,
#   ExceptionPath::matchesFirstFrom, matchesFirstThru, matchesLastTo,
#   ExceptionPath::resetMatch, ExceptionPath::nextState,
#   Sdc.cc addException, deleteException,
#   exceptionMatchFirst, exceptionMatchNext,
#   ExceptionState::nextState, ExceptionState::isComplete
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 1.0 [get_ports in1]
set_input_delay -clock clk1 1.0 [get_ports in2]
set_input_delay -clock clk2 2.0 [get_ports in3]
set_output_delay -clock clk1 2.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]

report_checks > /dev/null

############################################################
# Test 1: Multi-thru false paths with pin types
############################################################
puts "--- multi-thru state matching ---"

# 2-stage thru: pin then pin
set_false_path -from [get_ports in1] \
  -through [get_pins buf1/Z] \
  -through [get_pins and1/ZN] \
  -to [get_ports out1]
report_checks -path_delay max -from [get_ports in1] -to [get_ports out1]

# Another 2-stage thru
set_false_path -from [get_ports in2] \
  -through [get_pins inv1/ZN] \
  -through [get_pins nand1/ZN] \
  -to [get_ports out1]
report_checks -path_delay max -from [get_ports in2] -to [get_ports out1]

puts "--- unset multi-thru ---"
unset_path_exceptions -from [get_ports in1] -through [get_pins buf1/Z] \
  -through [get_pins and1/ZN] -to [get_ports out1]
unset_path_exceptions -from [get_ports in2] -through [get_pins inv1/ZN] \
  -through [get_pins nand1/ZN] -to [get_ports out1]
report_checks -path_delay max

############################################################
# Test 2: Exception filter matching (report_checks -from/-to/-through)
############################################################
puts "--- exception filter matching ---"

set_false_path -from [get_ports in1] -to [get_ports out2]
set_multicycle_path 2 -setup -from [get_clocks clk1] -to [get_clocks clk2]
set_max_delay 7.0 -from [get_ports in3] -to [get_ports out2]

puts "--- report_checks -from in1 ---"
report_checks -path_delay max -from [get_ports in1]

puts "--- report_checks -to out2 ---"
report_checks -path_delay max -to [get_ports out2]

puts "--- report_checks -from in3 -to out2 ---"
report_checks -path_delay max -from [get_ports in3] -to [get_ports out2]

puts "--- report_checks -through pin ---"
report_checks -path_delay max -through [get_pins buf1/Z]

puts "--- report_checks -through instance ---"
report_checks -path_delay max -through [get_cells and1]

############################################################
# Test 3: Unset exceptions
############################################################
puts "--- unset all exceptions ---"
unset_path_exceptions -from [get_ports in1] -to [get_ports out2]
unset_path_exceptions -from [get_clocks clk1] -to [get_clocks clk2]
unset_path_exceptions -from [get_ports in3] -to [get_ports out2]
report_checks -path_delay max

############################################################
# Test 4: group_path with filter
############################################################
puts "--- group_path filter ---"
group_path -name gp_in1 -from [get_ports in1]
group_path -name gp_out1 -to [get_ports out1]
group_path -name gp_thru -through [get_pins and1/ZN]

report_checks -path_delay max -path_group gp_in1
report_checks -path_delay max -path_group gp_out1
report_checks -path_delay max -path_group gp_thru

############################################################
# Test 5: From/to with instances
############################################################
puts "--- from instance ---"
set_false_path -from [get_cells reg1] -to [get_ports out2]
report_checks -path_delay max

unset_path_exceptions -from [get_cells reg1] -to [get_ports out2]

puts "--- to instance ---"
set_false_path -from [get_ports in1] -to [get_cells reg2]
report_checks -path_delay max

unset_path_exceptions -from [get_ports in1] -to [get_cells reg2]

############################################################
# Test 6: From/to with clock objects
############################################################
puts "--- from clock ---"
set_false_path -from [get_clocks clk1] -to [get_ports out2]
report_checks -path_delay max

unset_path_exceptions -from [get_clocks clk1] -to [get_ports out2]

############################################################
# Test 7: Rise/fall through
############################################################
puts "--- rise_through ---"
set_false_path -rise_through [get_pins buf1/Z] -to [get_ports out1]
report_checks -path_delay max

unset_path_exceptions -through [get_pins buf1/Z] -to [get_ports out1]

puts "--- fall_through ---"
set_false_path -fall_through [get_pins inv1/ZN] -to [get_ports out1]
report_checks -path_delay max

unset_path_exceptions -through [get_pins inv1/ZN] -to [get_ports out1]

############################################################
# Test 8: Write SDC roundtrip with complex exceptions
############################################################
set_false_path -rise_from [get_ports in1] -to [get_ports out1]
set_false_path -from [get_ports in2] -fall_to [get_ports out2]
set_multicycle_path 3 -setup -from [get_clocks clk1] -to [get_clocks clk2]

set sdc_out [make_result_file sdc_exc_match_filter.sdc]
write_sdc -no_timestamp $sdc_out

read_sdc $sdc_out

set sdc_out2 [make_result_file sdc_exc_match_filter2.sdc]
write_sdc -no_timestamp $sdc_out2
