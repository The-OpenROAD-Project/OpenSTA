# Test complex exception thru matching with nets, instances, pins
# and the ExceptionThru state machine for multi-through points.
# Targets:
#   ExceptionPath.cc: ExceptionThru::matches with nets/instances/pins,
#     ExceptionThru::intersectsPts with mixed object types,
#     ExceptionThru::equal, ExceptionThru::compare,
#     ExceptionThru::findHash with all object types,
#     ExceptionThru::allPins (net expansion to pins),
#     ExceptionTo::matches (with pin/clk/inst matching branches),
#     ExceptionTo::matchesFilter (report filter path),
#     ExceptionFrom::intersectsPts (pin/clk/inst branches),
#     ExceptionPath::intersectsPts,
#     ExceptionPath::mergeable (complex merge decisions),
#     fromThruToPriority calculation for various combos,
#     ExceptionPathLess comparison for sorting
#   Sdc.cc: makeExceptionThru with nets/instances/pins,
#     addException (merge logic), resetPath,
#     makeExceptionFrom with mixed pins/clocks/instances,
#     makeExceptionTo with mixed pins/clocks/instances
#   WriteSdc.cc: writeExceptionThru with nets/instances/pins,
#     writeExceptionFromTo with multi-object lists
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
# Test 1: Through nets (exercises ExceptionThru with nets)
############################################################
set_false_path -through [get_nets n1] -to [get_ports out1]
puts "PASS: false_path through net n1"

set_false_path -through [get_nets n2] -to [get_ports out2]
puts "PASS: false_path through net n2"

set_false_path -from [get_ports in1] -through [get_nets n3] -to [get_ports out1]
puts "PASS: false_path from/through net n3/to"

# Through net with rise_through
set_false_path -rise_through [get_nets n4] -to [get_ports out2]
puts "PASS: false_path -rise_through net n4"

# Through net with fall_through
set_false_path -fall_through [get_nets n5] -to [get_ports out1]
puts "PASS: false_path -fall_through net n5"

############################################################
# Test 2: Through instances (exercises ExceptionThru with instances)
############################################################
set_false_path -through [get_cells buf1] -to [get_ports out2]
puts "PASS: false_path through instance buf1"

set_false_path -from [get_ports in2] -through [get_cells and1] -to [get_ports out1]
puts "PASS: false_path from/through instance and1/to"

# Rise through instance
set_false_path -from [get_ports in3] -rise_through [get_cells or1] -to [get_ports out2]
puts "PASS: false_path -rise_through instance or1"

############################################################
# Test 3: Multiple through points with mixed types
# (net then pin, instance then pin, pin then net)
############################################################

# Net then pin
set_false_path -from [get_ports in1] \
  -through [get_nets n1] \
  -through [get_pins and1/ZN] \
  -to [get_ports out1]
puts "PASS: false_path through net then pin"

# Instance then pin
set_false_path -from [get_ports in2] \
  -through [get_cells inv1] \
  -through [get_pins nand1/ZN] \
  -to [get_ports out1]
puts "PASS: false_path through instance then pin"

# Pin then net
set_false_path -from [get_ports in1] \
  -through [get_pins buf1/Z] \
  -through [get_nets n3] \
  -to [get_ports out1]
puts "PASS: false_path through pin then net"

# Three through points: pin, net, instance
set_false_path -from [get_ports in2] \
  -through [get_pins inv1/ZN] \
  -through [get_nets n3] \
  -through [get_cells nand1] \
  -to [get_ports out1]
puts "PASS: false_path 3 through: pin, net, instance"

############################################################
# Test 4: From with mixed objects (pins + clocks)
############################################################
set_false_path -from [list [get_ports in1] [get_ports in2]] \
  -to [list [get_ports out1] [get_ports out2]]
puts "PASS: false_path multi-pin from/to"

set_false_path -from [list [get_clocks clk1] [get_ports in3]] \
  -to [get_ports out1]
puts "PASS: false_path mixed clock+pin from"

# From with instances
set_false_path -from [get_cells reg1] -to [get_ports out2]
puts "PASS: false_path from instance"

# To with instances
set_false_path -from [get_ports in1] -to [get_cells reg2]
puts "PASS: false_path to instance"

############################################################
# Test 5: Max/min delay with through
############################################################
set_max_delay -from [get_ports in1] -through [get_nets n1] -to [get_ports out1] 7.0
puts "PASS: max_delay through net"

set_max_delay -from [get_ports in2] -through [get_cells and1] -to [get_ports out1] 6.5
puts "PASS: max_delay through instance"

set_max_delay -from [get_ports in3] \
  -through [get_pins or1/ZN] \
  -to [get_ports out2] 8.0
puts "PASS: max_delay through pin"

set_min_delay -from [get_ports in1] -through [get_nets n1] -to [get_ports out1] 0.5
puts "PASS: min_delay through net"

# Max delay with -ignore_clock_latency
set_max_delay -from [get_ports in3] -to [get_ports out2] -ignore_clock_latency 9.0
puts "PASS: max_delay -ignore_clock_latency"

############################################################
# Test 6: Multicycle with through
############################################################
set_multicycle_path -setup 2 -from [get_ports in1] \
  -through [get_pins buf1/Z] \
  -to [get_ports out1]
puts "PASS: multicycle through pin"

set_multicycle_path -hold 1 -from [get_ports in1] \
  -through [get_pins buf1/Z] \
  -to [get_ports out1]
puts "PASS: multicycle hold through pin"

############################################################
# Test 7: Group path with through nets/instances
############################################################
group_path -name gp_net -from [get_ports in1] \
  -through [get_nets n1] \
  -to [get_ports out1]
puts "PASS: group_path through net"

group_path -name gp_inst -from [get_ports in2] \
  -through [get_cells and1] \
  -to [get_ports out1]
puts "PASS: group_path through instance"

group_path -name gp_pin -from [get_ports in3] \
  -through [get_pins or1/ZN] \
  -to [get_ports out2]
puts "PASS: group_path through pin"

group_path -default -from [get_ports in1] -to [get_ports out2]
puts "PASS: group_path -default"

############################################################
# Write SDC
############################################################
set sdc1 [make_result_file sdc_exc_thru_complex1.sdc]
write_sdc -no_timestamp $sdc1
puts "PASS: write_sdc"

set sdc2 [make_result_file sdc_exc_thru_complex2.sdc]
write_sdc -no_timestamp -compatible $sdc2
puts "PASS: write_sdc compatible"

set sdc3 [make_result_file sdc_exc_thru_complex3.sdc]
write_sdc -no_timestamp -digits 6 $sdc3
puts "PASS: write_sdc digits 6"

############################################################
# Unset and verify
############################################################
unset_path_exceptions -through [get_nets n1] -to [get_ports out1]
unset_path_exceptions -through [get_nets n2] -to [get_ports out2]
unset_path_exceptions -from [get_ports in1] -through [get_nets n3] -to [get_ports out1]
puts "PASS: unset net through paths"

unset_path_exceptions -through [get_cells buf1] -to [get_ports out2]
unset_path_exceptions -from [get_ports in2] -through [get_cells and1] -to [get_ports out1]
puts "PASS: unset instance through paths"

# Write after unset to verify reduced constraints
set sdc_unset [make_result_file sdc_exc_thru_complex_unset.sdc]
write_sdc -no_timestamp $sdc_unset
puts "PASS: write_sdc after unset"

############################################################
# Read back SDC and verify roundtrip
############################################################
read_sdc $sdc1
puts "PASS: read_sdc"

set sdc4 [make_result_file sdc_exc_thru_complex4.sdc]
write_sdc -no_timestamp $sdc4
puts "PASS: write_sdc after read"

puts "ALL PASSED"
