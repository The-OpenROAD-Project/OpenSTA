# Test exception paths with through nets and instances,
# multi-object from/to, group_path default, and write_sdc
# exercises exception writing with nets/instances.
# Targets:
#   ExceptionPath.cc: ExceptionPath constructor, fromThruToPriority,
#     firstPt, matchesFirstPt, asString, typeString,
#     ExceptionFrom/To/Thru with instances/nets/clks combinations,
#     ExceptionPathLess comparison, checkFromThrusTo,
#     setPriority, setId
#   Sdc.cc: makeFalsePath, makeMulticyclePath, makePathDelay,
#     makeGroupPath (default), resetPath, makeExceptionFrom/Thru/To,
#     deleteExceptionFrom/Thru/To, checkExceptionFromPins/ToPins,
#     isPathGroupName
#   WriteSdc.cc: writeException, writeExceptionCmd (all types),
#     writeExceptionFrom/To/Thru with multi-objects,
#     writeExceptionValue (multicycle multiplier, delay value),
#     writeGroupPath (default and named),
#     mapThruHpins
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
# False path with through nets
############################################################
set_false_path -through [get_nets n1] -to [get_ports out1]
puts "PASS: false_path through net"

set_false_path -from [get_ports in2] -through [get_nets n2] -to [get_ports out1]
puts "PASS: false_path from/through net/to"

# False path through net and pin combined
set_false_path -from [get_ports in1] -through [get_nets n3] -through [get_pins nand1/ZN] -to [get_ports out1]
puts "PASS: false_path through net+pin"

############################################################
# False path with through instances
############################################################
set_false_path -from [get_ports in1] -through [get_cells buf1] -to [get_ports out2]
puts "PASS: false_path through instance"

############################################################
# False path with from instances
############################################################
set_false_path -from [get_cells reg1] -to [get_ports out2]
puts "PASS: false_path from instance"

############################################################
# False path with to instances
############################################################
set_false_path -from [get_ports in1] -to [get_cells reg2]
puts "PASS: false_path to instance"

############################################################
# Multi-object from/to
############################################################
set_false_path -from [list [get_ports in1] [get_ports in2]] \
    -to [list [get_ports out1] [get_ports out2]]
puts "PASS: false_path multi-object"

# Multi-object with clocks
set_false_path -setup -from [list [get_clocks clk1] [get_ports in3]] \
    -to [get_ports out1]
puts "PASS: false_path multi from (clk + pin)"

############################################################
# Write to verify exception writing
############################################################
set sdc1 [make_result_file sdc_exception_thru1.sdc]
write_sdc -no_timestamp $sdc1
puts "PASS: write_sdc with net/inst exceptions"

############################################################
# Unset all false paths and create new ones
############################################################
unset_path_exceptions -through [get_nets n1] -to [get_ports out1]
unset_path_exceptions -from [get_ports in2] -through [get_nets n2] -to [get_ports out1]
unset_path_exceptions -from [get_ports in1] -through [get_nets n3] -through [get_pins nand1/ZN] -to [get_ports out1]
unset_path_exceptions -from [get_ports in1] -through [get_cells buf1] -to [get_ports out2]
unset_path_exceptions -from [get_cells reg1] -to [get_ports out2]
unset_path_exceptions -from [get_ports in1] -to [get_cells reg2]
puts "PASS: unset exceptions"

############################################################
# Max/min delay with -ignore_clock_latency
############################################################
set_max_delay -from [get_ports in1] -to [get_ports out1] -ignore_clock_latency 8.0
puts "PASS: max_delay -ignore_clock_latency"

set_min_delay -from [get_ports in1] -to [get_ports out1] 1.0
puts "PASS: min_delay"

# Max delay with through net
set_max_delay -from [get_ports in2] -through [get_nets n2] -to [get_ports out1] 6.0
puts "PASS: max_delay through net"

# Max delay with through instance
set_max_delay -from [get_ports in3] -through [get_cells or1] -to [get_ports out2] 7.0
puts "PASS: max_delay through instance"

############################################################
# Multicycle path with -start/-end
############################################################
set_multicycle_path -setup -start 3 -from [get_ports in1] -to [get_ports out1]
puts "PASS: multicycle -setup -start"

set_multicycle_path -hold -end 1 -from [get_ports in1] -to [get_ports out1]
puts "PASS: multicycle -hold -end"

############################################################
# Group path - default
############################################################
group_path -default -from [get_ports in1] -to [get_ports out1]
puts "PASS: group_path -default"

# Named group path with through
group_path -name grp_thru -from [get_ports in2] -through [get_pins and1/ZN] -to [get_ports out1]
puts "PASS: group_path with through"

############################################################
# Write with all exception types
############################################################
set sdc2 [make_result_file sdc_exception_thru2.sdc]
write_sdc -no_timestamp $sdc2
puts "PASS: write_sdc with all exception types"

set sdc3 [make_result_file sdc_exception_thru3.sdc]
write_sdc -no_timestamp -compatible $sdc3
puts "PASS: write_sdc compatible"

############################################################
# Group path names query
############################################################
set gp_names [sta::group_path_names]
puts "group_path_names = $gp_names"
puts "PASS: group_path_names"

############################################################
# is_path_group_name
############################################################
set is_gp [sta::is_path_group_name "grp_thru"]
puts "is_path_group_name grp_thru = $is_gp"

set is_gp [sta::is_path_group_name "nonexistent"]
puts "is_path_group_name nonexistent = $is_gp"
puts "PASS: is_path_group_name"

puts "ALL PASSED"
