# Test exception path override logic, priority resolution between
# false_path/multicycle/max_delay/group_path, and complex thru combinations.
# Targets:
#   ExceptionPath.cc: FalsePath::overrides, MultiCyclePath::overrides,
#     PathDelay::overrides, GroupPath::overrides,
#     FalsePath::mergeable, MultiCyclePath::mergeable,
#     PathDelay::mergeable, GroupPath::mergeable,
#     ExceptionPath::fromThruToPriority (various priority combos),
#     ExceptionPathLess comparison (sort by type priority),
#     FalsePath::clone, MultiCyclePath::clone, PathDelay::clone,
#     ExceptionTo::matchesFilter with endTransition rise/fall,
#     ExceptionTo::intersectsPts, ExceptionFrom::intersectsPts,
#     ExceptionThru::equal, ExceptionThru::compare,
#     ExceptionThru::findHash with nets/instances/pins,
#     checkFromThrusTo
#   Sdc.cc: addException (merge and override logic),
#     makeExceptionFrom/To/Thru with various object combos,
#     deleteExceptionsReferencing
#   WriteSdc.cc: writeExceptions (all types with priority ordering),
#     writeExceptionCmd, writeExceptionValue, writeGroupPath
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
create_clock -name vclk -period 8
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_input_delay -clock clk2 2.0 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]

############################################################
# Test 1: Override max_delay with false_path
# (FalsePath::overrides, PathDelay::overrides)
############################################################

# Set max_delay first
set_max_delay -from [get_ports in1] -to [get_ports out1] 8.0

# Override with false_path on same from/to
set_false_path -from [get_ports in1] -to [get_ports out1]

# Set min_delay on different path
set_min_delay -from [get_ports in2] -to [get_ports out1] 1.0

# Override min_delay with another min_delay (same endpoints)
set_min_delay -from [get_ports in2] -to [get_ports out1] 2.0

############################################################
# Test 2: Multicycle path overrides
# (MultiCyclePath::overrides, MultiCyclePath::mergeable)
############################################################

# Setup multicycle
set_multicycle_path -setup 2 -from [get_ports in1] -to [get_ports out2]

# Override with different multiplier (same endpoints, same type)
set_multicycle_path -setup 3 -from [get_ports in1] -to [get_ports out2]

# Hold multicycle on same path
set_multicycle_path -hold 1 -from [get_ports in1] -to [get_ports out2]

# Override hold with different value
set_multicycle_path -hold 2 -from [get_ports in1] -to [get_ports out2]

# Multicycle with -start (exercises use_end_clk=false)
set_multicycle_path -setup -start 4 -from [get_ports in2] -to [get_ports out2]

# Multicycle with -end (exercises use_end_clk=true)
set_multicycle_path -hold -end 2 -from [get_ports in2] -to [get_ports out2]

############################################################
# Test 3: Exception with rise/fall transitions on to/from
# (ExceptionTo::matchesFilter with endTransition)
############################################################

# False path with rise_from only
set_false_path -rise_from [get_ports in3] -to [get_ports out1]

# False path with fall_from only
set_false_path -fall_from [get_ports in3] -to [get_ports out2]

# False path with rise_to
set_false_path -from [get_ports in2] -rise_to [get_ports out1]

# False path with fall_to
set_false_path -from [get_ports in2] -fall_to [get_ports out2]

# Multicycle with rise_from/fall_to
set_multicycle_path -setup 2 -rise_from [get_clocks clk1] -to [get_clocks clk2]

set_multicycle_path -setup 3 -from [get_clocks clk1] -fall_to [get_clocks clk2]

############################################################
# Test 4: Group path overrides
# (GroupPath::overrides, GroupPath::mergeable)
############################################################

# Named group path
group_path -name grp_a -from [get_ports in1] -to [get_ports out1]

# Override with same name (exercises GroupPath::overrides)
group_path -name grp_a -from [get_ports in1] -to [get_ports out2]

# Default group path
group_path -default -from [get_ports in3] -to [get_ports out2]

# Another default (exercises isDefault override logic)
group_path -default -from [get_ports in3] -to [get_ports out1]

# Group path with through
group_path -name grp_thru \
  -from [get_ports in1] \
  -through [get_pins buf1/Z] \
  -to [get_ports out1]

# Group path with through net
group_path -name grp_net \
  -from [get_ports in2] \
  -through [get_nets n2] \
  -to [get_ports out1]

# Group path with through instance
group_path -name grp_inst \
  -from [get_ports in1] \
  -through [get_cells and1] \
  -to [get_ports out2]

############################################################
# Test 5: Complex through combinations
# (ExceptionThru with pins + nets + instances)
############################################################

# Multiple through points: pin then net then pin
set_false_path -from [get_ports in1] \
  -through [get_pins buf1/Z] \
  -through [get_nets n3] \
  -through [get_pins nand1/ZN] \
  -to [get_ports out1]

# Rise_through and fall_through combined
set_false_path -from [get_ports in2] \
  -rise_through [get_pins and1/ZN] \
  -fall_through [get_pins nand1/ZN] \
  -to [get_ports out1]

# Max delay with through instance
set_max_delay -from [get_ports in3] \
  -through [get_cells or1] \
  -to [get_ports out2] 7.0

# Max delay with through net
set_max_delay -from [get_ports in1] \
  -through [get_nets n1] \
  -to [get_ports out1] 6.0

############################################################
# Test 6: False path with -setup and -hold only
############################################################
set_false_path -setup -from [get_clocks clk1] -to [get_clocks clk2]

set_false_path -hold -from [get_clocks clk2] -to [get_clocks clk1]

############################################################
# Write SDC with all exception types
############################################################
set sdc1 [make_result_file sdc_exc_override1.sdc]
write_sdc -no_timestamp $sdc1
diff_files sdc_exc_override1.sdcok $sdc1

set sdc2 [make_result_file sdc_exc_override2.sdc]
write_sdc -no_timestamp -compatible $sdc2
diff_files sdc_exc_override2.sdcok $sdc2

set sdc3 [make_result_file sdc_exc_override3.sdc]
write_sdc -no_timestamp -digits 6 $sdc3
diff_files sdc_exc_override3.sdcok $sdc3

############################################################
# Unset some exceptions and verify
############################################################
unset_path_exceptions -from [get_ports in1] -to [get_ports out1]

unset_path_exceptions -from [get_ports in2] -rise_to [get_ports out1]
unset_path_exceptions -from [get_ports in2] -fall_to [get_ports out2]

# Write after unset to exercise writing with reduced exceptions
set sdc_unset [make_result_file sdc_exc_override_unset.sdc]
write_sdc -no_timestamp $sdc_unset
diff_files sdc_exc_override_unset.sdcok $sdc_unset

############################################################
# Read back and verify roundtrip
############################################################
read_sdc $sdc1

set sdc4 [make_result_file sdc_exc_override4.sdc]
write_sdc -no_timestamp $sdc4
diff_files sdc_exc_override4.sdcok $sdc4
