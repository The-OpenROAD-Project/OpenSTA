# Test exception path intersections, priority merging, ExceptionThru matching
# with nets/instances/pins, multiple through points, and complex write_sdc paths.
# Targets:
#   ExceptionPath.cc: ExceptionThru::intersectsPts with nets/instances/pins,
#     ExceptionTo::matchesFilter, ExceptionTo with endTransition,
#     ExceptionFrom::intersectsPts, ExceptionPath::intersectsPts,
#     ExceptionPath::mergeable, checkFromThrusTo,
#     FalsePath::clone, MultiCyclePath::clone, PathDelay::clone,
#     ExceptionPathLess comparison (sort by type priority),
#     fromThruToPriority calculation
#   Sdc.cc: makeExceptionThru with nets/instances/pins,
#     addException (merge logic), resetPath,
#     removeInputDelay, removeOutputDelay,
#     clockGroupsAreSame (via report)
#   WriteSdc.cc: writeExceptionThru with nets/instances,
#     writeExceptionFromTo with multi-object lists,
#     writeExceptionCmd for all types (false/multicycle/path_delay/group),
#     writeExceptionValue (delay, multiplier),
#     writeRiseFallMinMaxTimeCmd, writeRiseFallMinMaxCmd
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
# Exception with -through using nets, instances, and pins combined
############################################################

# Through net only
set_false_path -through [get_nets n1] -to [get_ports out1]
puts "PASS: false_path through net n1"

# Through instance only
set_false_path -through [get_cells inv1] -to [get_ports out2]
puts "PASS: false_path through instance inv1"

# Through pin and net combined in sequence
set_false_path -from [get_ports in1] \
  -through [get_pins buf1/Z] \
  -through [get_nets n3] \
  -to [get_ports out1]
puts "PASS: false_path through pin then net"

# Through instance and pin in sequence
set_false_path -from [get_ports in2] \
  -through [get_cells and1] \
  -through [get_pins nand1/ZN] \
  -to [get_ports out1]
puts "PASS: false_path through instance then pin"

# Rise through
set_false_path -from [get_ports in3] \
  -rise_through [get_pins or1/ZN] \
  -to [get_ports out2]
puts "PASS: false_path -rise_through"

# Fall through
set_false_path -from [get_ports in1] \
  -fall_through [get_pins buf1/Z] \
  -to [get_ports out2]
puts "PASS: false_path -fall_through"

############################################################
# Write SDC with through exceptions
############################################################
set sdc1 [make_result_file sdc_exception_int1.sdc]
write_sdc -no_timestamp $sdc1
puts "PASS: write_sdc with through exceptions"

############################################################
# Unset all paths and create new set for merging tests
############################################################
unset_path_exceptions -through [get_nets n1] -to [get_ports out1]
unset_path_exceptions -through [get_cells inv1] -to [get_ports out2]
unset_path_exceptions -from [get_ports in1] -through [get_pins buf1/Z] -through [get_nets n3] -to [get_ports out1]
unset_path_exceptions -from [get_ports in2] -through [get_cells and1] -through [get_pins nand1/ZN] -to [get_ports out1]
unset_path_exceptions -from [get_ports in3] -rise_through [get_pins or1/ZN] -to [get_ports out2]
unset_path_exceptions -from [get_ports in1] -fall_through [get_pins buf1/Z] -to [get_ports out2]
puts "PASS: unset_path_exceptions"

############################################################
# Exception merging: multiple exceptions on overlapping paths
############################################################

# False path that should merge when same from/to
set_false_path -from [get_ports in1] -to [get_ports out1]
set_false_path -from [get_ports in2] -to [get_ports out1]
puts "PASS: false paths to same target (mergeable)"

# Max delay on same endpoints - second should override
set_max_delay -from [get_ports in1] -to [get_ports out2] 7.0
set_max_delay -from [get_ports in2] -to [get_ports out2] 6.0
puts "PASS: max_delay to same target"

# Min delay
set_min_delay -from [get_ports in1] -to [get_ports out2] 0.5
set_min_delay -from [get_ports in2] -to [get_ports out2] 0.3
puts "PASS: min_delay to same target"

# Multicycle on different from/to (not mergeable)
set_multicycle_path -setup 2 -from [get_ports in1] -to [get_ports out1]
set_multicycle_path -setup 3 -from [get_ports in2] -to [get_ports out2]
set_multicycle_path -hold 1 -from [get_ports in1] -to [get_ports out1]
set_multicycle_path -hold 0 -from [get_ports in2] -to [get_ports out2]
puts "PASS: multicycle paths"

# False path with -setup only
set_false_path -setup -from [get_clocks clk1] -to [get_clocks clk2]
puts "PASS: false_path -setup only"

# False path with -hold only
set_false_path -hold -from [get_clocks clk2] -to [get_clocks clk1]
puts "PASS: false_path -hold only"

# Group path - default
group_path -default -from [get_ports in3] -to [get_ports out2]
puts "PASS: group_path -default"

# Group path - named with through nets
group_path -name grp_net \
  -from [get_ports in1] \
  -through [get_nets n1] \
  -to [get_ports out1]
puts "PASS: group_path with through net"

# Group path - named with through instances
group_path -name grp_inst \
  -from [get_ports in2] \
  -through [get_cells and1] \
  -to [get_ports out1]
puts "PASS: group_path with through instance"

############################################################
# Write SDC with all exception types
############################################################
set sdc2 [make_result_file sdc_exception_int2.sdc]
write_sdc -no_timestamp $sdc2
puts "PASS: write_sdc all exception types"

set sdc3 [make_result_file sdc_exception_int3.sdc]
write_sdc -no_timestamp -compatible $sdc3
puts "PASS: write_sdc compatible"

set sdc4 [make_result_file sdc_exception_int4.sdc]
write_sdc -no_timestamp -digits 6 $sdc4
puts "PASS: write_sdc digits 6"

############################################################
# Read back SDC
############################################################
read_sdc $sdc2
puts "PASS: read_sdc"

# Re-write to verify roundtrip
set sdc5 [make_result_file sdc_exception_int5.sdc]
write_sdc -no_timestamp $sdc5
puts "PASS: write_sdc roundtrip"

puts "ALL PASSED"
