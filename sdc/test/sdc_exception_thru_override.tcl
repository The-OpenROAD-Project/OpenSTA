# Test ExceptionPath.cc: exception path overriding, priority ordering,
# intersectsPts with rise/fall transitions, equal/compare/hash operations,
# and exception merging with complex thru point combinations.
# Targets: ExceptionPath.cc ExceptionPath::matches, overrides,
#   ExceptionPath::intersectsPts with rise/fall pins,
#   ExceptionPath::mergeable, ExceptionPath::tighterThan,
#   ExceptionPathLess comparison, ExceptionPath::resetMatch,
#   ExceptionTo::matches with rise/fall/instance matching,
#   ExceptionFrom::matches with clock/rise/fall,
#   ExceptionThru::matches with net/pin/instance combinations,
#   fromThruToPriority with different specificity levels,
#   Sdc.cc addException (merge/override), deleteException,
#   makeExceptionFrom with rise_from/fall_from,
#   makeExceptionTo with rise_to/fall_to
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 15 [get_ports clk2]
set_input_delay -clock clk1 1.0 [get_ports in1]
set_input_delay -clock clk1 1.0 [get_ports in2]
set_input_delay -clock clk2 2.0 [get_ports in3]
set_output_delay -clock clk1 2.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]

report_checks > /dev/null

############################################################
# Test 1: Rise/fall from exceptions
############################################################
puts "--- set_false_path -rise_from ---"
set_false_path -rise_from [get_ports in1] -to [get_ports out1]
report_checks -path_delay max -from [get_ports in1]

puts "--- set_false_path -fall_from ---"
set_false_path -fall_from [get_ports in2] -to [get_ports out1]
report_checks -path_delay max -from [get_ports in2]

puts "--- unset rise/fall from ---"
unset_path_exceptions -from [get_ports in1] -to [get_ports out1]
unset_path_exceptions -from [get_ports in2] -to [get_ports out1]

############################################################
# Test 2: Rise/fall to exceptions
############################################################
puts "--- set_false_path -rise_to ---"
set_false_path -from [get_ports in1] -rise_to [get_ports out1]
report_checks -path_delay max -to [get_ports out1]

puts "--- set_false_path -fall_to ---"
set_false_path -from [get_ports in2] -fall_to [get_ports out1]
report_checks -path_delay max -from [get_ports in2]

puts "--- unset rise/fall to ---"
unset_path_exceptions -from [get_ports in1] -to [get_ports out1]
unset_path_exceptions -from [get_ports in2] -to [get_ports out1]

############################################################
# Test 3: Rise/fall through exceptions
############################################################
puts "--- set_false_path -rise_through ---"
set_false_path -rise_through [get_pins buf1/Z] -to [get_ports out1]
report_checks -path_delay max

puts "--- set_false_path -fall_through ---"
set_false_path -fall_through [get_pins inv1/ZN] -to [get_ports out1]
report_checks -path_delay max

puts "--- unset rise/fall through ---"
unset_path_exceptions -through [get_pins buf1/Z] -to [get_ports out1]
unset_path_exceptions -through [get_pins inv1/ZN] -to [get_ports out1]

############################################################
# Test 4: Exception priority ordering (more specific overrides less specific)
############################################################
puts "--- priority: broad false_path ---"
set_false_path -from [get_ports in1]
report_checks -path_delay max -from [get_ports in1]

puts "--- priority: narrower multicycle overriding false_path ---"
set_multicycle_path 2 -setup -from [get_ports in1] -to [get_ports out1]
report_checks -path_delay max -from [get_ports in1] -to [get_ports out1]

puts "--- priority: most specific max_delay ---"
set_max_delay 5.0 -from [get_ports in1] -through [get_pins buf1/Z] -to [get_ports out1]
report_checks -path_delay max -from [get_ports in1] -through [get_pins buf1/Z] -to [get_ports out1]

puts "--- unset all ---"
unset_path_exceptions -from [get_ports in1]
unset_path_exceptions -from [get_ports in1] -to [get_ports out1]
unset_path_exceptions -from [get_ports in1] -through [get_pins buf1/Z] -to [get_ports out1]

############################################################
# Test 5: Clock-based from/to exceptions
############################################################
puts "--- false_path from clock ---"
set_false_path -from [get_clocks clk1] -to [get_clocks clk2]
report_checks -path_delay max

puts "--- unset clock false_path ---"
unset_path_exceptions -from [get_clocks clk1] -to [get_clocks clk2]
report_checks -path_delay max

puts "--- false_path -rise_from clock ---"
set_false_path -rise_from [get_clocks clk1] -to [get_clocks clk2]
report_checks -path_delay max

puts "--- unset rise_from clock ---"
unset_path_exceptions -from [get_clocks clk1] -to [get_clocks clk2]

############################################################
# Test 6: Multiple overlapping exceptions (merge testing)
############################################################
puts "--- overlapping exceptions ---"
set_false_path -from [get_ports in1] -to [get_ports out1]
set_false_path -from [get_ports in1] -to [get_ports out2]
set_false_path -from [get_ports in2] -to [get_ports out1]
report_checks -path_delay max

puts "--- unset overlapping ---"
unset_path_exceptions -from [get_ports in1] -to [get_ports out1]
unset_path_exceptions -from [get_ports in1] -to [get_ports out2]
unset_path_exceptions -from [get_ports in2] -to [get_ports out1]
report_checks -path_delay max

############################################################
# Test 7: Multicycle with -start/-end and rise/fall
############################################################
puts "--- mcp -start -rise_from ---"
set_multicycle_path 2 -setup -start -rise_from [get_clocks clk1] -to [get_clocks clk2]
report_checks -path_delay max

puts "--- mcp -end -fall_to ---"
set_multicycle_path 3 -setup -end -from [get_clocks clk1] -fall_to [get_clocks clk2]
report_checks -path_delay max

puts "--- unset mcp ---"
unset_path_exceptions -from [get_clocks clk1] -to [get_clocks clk2]

############################################################
# Test 8: Max/min delay with rise/fall from/to
############################################################
puts "--- max_delay -rise_from -to ---"
set_max_delay 6.0 -rise_from [get_ports in1] -to [get_ports out1]
report_checks -path_delay max

puts "--- min_delay -from -fall_to ---"
set_min_delay 0.1 -from [get_ports in1] -fall_to [get_ports out1]
report_checks -path_delay min

puts "--- unset max/min delays ---"
unset_path_exceptions -from [get_ports in1] -to [get_ports out1]

############################################################
# Test 9: write_sdc with exception paths
############################################################
set_false_path -rise_from [get_ports in1] -to [get_ports out1]
set_false_path -from [get_ports in2] -fall_to [get_ports out2]
set_multicycle_path 2 -setup -from [get_clocks clk1] -to [get_clocks clk2]
set_max_delay 7.0 -from [get_ports in3] -rise_through [get_pins or1/ZN] -to [get_ports out2]

set sdc1 [make_result_file sdc_exc_override1.sdc]
write_sdc -no_timestamp $sdc1

set sdc2 [make_result_file sdc_exc_override2.sdc]
write_sdc -no_timestamp -compatible $sdc2

############################################################
# Test 10: Read back SDC
############################################################
read_sdc $sdc1

set sdc3 [make_result_file sdc_exc_override3.sdc]
write_sdc -no_timestamp $sdc3
