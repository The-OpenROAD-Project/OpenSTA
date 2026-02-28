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

############################################################
# False path with through nets
############################################################
set_false_path -through [get_nets n1] -to [get_ports out1]

set_false_path -from [get_ports in2] -through [get_nets n2] -to [get_ports out1]

# False path through net and pin combined
set_false_path -from [get_ports in1] -through [get_nets n3] -through [get_pins nand1/ZN] -to [get_ports out1]

############################################################
# False path with through instances
############################################################
set_false_path -from [get_ports in1] -through [get_cells buf1] -to [get_ports out2]

############################################################
# False path with from instances
############################################################
set_false_path -from [get_cells reg1] -to [get_ports out2]

############################################################
# False path with to instances
############################################################
set_false_path -from [get_ports in1] -to [get_cells reg2]

############################################################
# Multi-object from/to
############################################################
set_false_path -from [list [get_ports in1] [get_ports in2]] \
    -to [list [get_ports out1] [get_ports out2]]

# Multi-object with clocks
set_false_path -setup -from [list [get_clocks clk1] [get_ports in3]] \
    -to [get_ports out1]

############################################################
# Write to verify exception writing
############################################################
set sdc1 [make_result_file sdc_exception_thru1.sdc]
write_sdc -no_timestamp $sdc1
diff_files sdc_exception_thru1.sdcok $sdc1

############################################################
# Unset all false paths and create new ones
############################################################
unset_path_exceptions -through [get_nets n1] -to [get_ports out1]
unset_path_exceptions -from [get_ports in2] -through [get_nets n2] -to [get_ports out1]
unset_path_exceptions -from [get_ports in1] -through [get_nets n3] -through [get_pins nand1/ZN] -to [get_ports out1]
unset_path_exceptions -from [get_ports in1] -through [get_cells buf1] -to [get_ports out2]
unset_path_exceptions -from [get_cells reg1] -to [get_ports out2]
unset_path_exceptions -from [get_ports in1] -to [get_cells reg2]

############################################################
# Max/min delay with -ignore_clock_latency
############################################################
set_max_delay -from [get_ports in1] -to [get_ports out1] -ignore_clock_latency 8.0

set_min_delay -from [get_ports in1] -to [get_ports out1] 1.0

# Max delay with through net
set_max_delay -from [get_ports in2] -through [get_nets n2] -to [get_ports out1] 6.0

# Max delay with through instance
set_max_delay -from [get_ports in3] -through [get_cells or1] -to [get_ports out2] 7.0

############################################################
# Multicycle path with -start/-end
############################################################
set_multicycle_path -setup -start 3 -from [get_ports in1] -to [get_ports out1]

set_multicycle_path -hold -end 1 -from [get_ports in1] -to [get_ports out1]

############################################################
# Group path - default
############################################################
group_path -default -from [get_ports in1] -to [get_ports out1]

# Named group path with through
group_path -name grp_thru -from [get_ports in2] -through [get_pins and1/ZN] -to [get_ports out1]

############################################################
# Write with all exception types
############################################################
set sdc2 [make_result_file sdc_exception_thru2.sdc]
write_sdc -no_timestamp $sdc2
diff_files sdc_exception_thru2.sdcok $sdc2

set sdc3 [make_result_file sdc_exception_thru3.sdc]
write_sdc -no_timestamp -compatible $sdc3
diff_files sdc_exception_thru3.sdcok $sdc3

############################################################
# Group path names query
############################################################
set gp_names [sta::group_path_names]
puts "group_path_names = $gp_names"

############################################################
# is_path_group_name
############################################################
set is_gp [sta::is_path_group_name "grp_thru"]
puts "is_path_group_name grp_thru = $is_gp"

set is_gp [sta::is_path_group_name "nonexistent"]
puts "is_path_group_name nonexistent = $is_gp"
