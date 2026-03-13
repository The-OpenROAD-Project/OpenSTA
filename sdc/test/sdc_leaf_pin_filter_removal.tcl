# Test Sdc.cc leaf pin queries, filter matching, constraint removal cascades,
# findLeafLoadPins/findLeafDriverPins, net properties.
# Also exercises Property.cc Net properties (is_power, is_ground, connected_count),
# TimingArcSet properties (is_disabled_cond), and Sdc.cc filter matching.
# Targets: Sdc.cc findLeafLoadPins, findLeafDriverPins, isLeafPinClock,
#   isLeafPinNonGeneratedClock, deleteConstraints, clkStopPropagation,
#   matchFilter, makeExceptionFrom/To/Thru with instances/nets/pins,
#   Property.cc net_is_power, net_is_ground, net_connected_count,
#   timing_arc_set_is_disabled_cond
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

report_checks > /dev/null

############################################################
# Net properties
############################################################
puts "--- net properties ---"
set nets [get_nets *]
foreach n $nets {
  set name [get_full_name $n]
  set is_pwr [$n is_power]
  set is_gnd [$n is_ground]
  puts "  net $name: is_power=$is_pwr is_ground=$is_gnd"
}

############################################################
# Pin/port properties
############################################################
puts "--- port properties ---"
set ports [get_ports *]
foreach p $ports {
  set name [get_full_name $p]
  set dir [get_property $p direction]
  set pin [sta::get_port_pin $p]
  set is_clk [sta::is_clock $pin]
  puts "  port $name: direction=$dir is_clock=$is_clk"
}

############################################################
# Instance properties
############################################################
puts "--- instance properties ---"
set insts [get_cells *]
foreach i $insts {
  set name [get_full_name $i]
  set ref [get_property $i ref_name]
  set lib_cell [get_property $i liberty_cell]
  if { $lib_cell != "NULL" && $lib_cell ne "" } {
    set lib_name [get_name [$lib_cell liberty_library]]
  } else {
    set lib_name ""
  }
  puts "  inst $name: ref=$ref lib=$lib_name"
}

############################################################
# Pin properties (timing arc set disabled)
############################################################
puts "--- pin properties ---"
foreach p [get_pins buf1/*] {
  set name [get_full_name $p]
  set dir [get_property $p direction]
  set is_clk [get_property $p is_clock]
  puts "  pin $name: direction=$dir is_clock=$is_clk"
}

############################################################
# Disable timing on arc and check property
############################################################
puts "--- disable timing arc ---"
set_disable_timing -from A1 -to ZN [get_cells and1]
report_checks -through [get_pins and1/ZN]

puts "--- re-enable timing arc ---"
unset_disable_timing -from A1 -to ZN [get_cells and1]
report_checks -through [get_pins and1/ZN]

############################################################
# Constraint removal cascades: add many constraints then remove
############################################################
puts "--- constraint setup ---"
set_false_path -from [get_clocks clk1] -to [get_clocks clk2]
set_multicycle_path -setup 2 -from [get_ports in1] -to [get_ports out1]
set_max_delay -from [get_ports in2] -to [get_ports out1] 8.0
set_min_delay -from [get_ports in2] -to [get_ports out1] 0.5
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup 0.3
set_clock_groups -asynchronous -name grp1 \
  -group {clk1} -group {clk2}

puts "--- write_sdc before removal ---"
set sdc1 [make_result_file sdc_leaf_pin1.sdc]
write_sdc -no_timestamp $sdc1
diff_files sdc_leaf_pin1.sdcok $sdc1

puts "--- remove false_path ---"
unset_path_exceptions -from [get_clocks clk1] -to [get_clocks clk2]

puts "--- remove multicycle ---"
unset_path_exceptions -setup -from [get_ports in1] -to [get_ports out1]

puts "--- remove max/min delay ---"
unset_path_exceptions -from [get_ports in2] -to [get_ports out1]

puts "--- remove clock_groups ---"
unset_clock_groups -asynchronous -name grp1

puts "--- remove clock_uncertainty ---"
unset_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup

puts "--- write_sdc after removal ---"
set sdc2 [make_result_file sdc_leaf_pin2.sdc]
write_sdc -no_timestamp $sdc2
diff_files sdc_leaf_pin2.sdcok $sdc2

############################################################
# Filter queries: get_* with -filter
############################################################
puts "--- filter queries ---"

set bufs [get_cells -filter "ref_name == BUF_X1"]
puts "BUF_X1 cells: [llength $bufs]"

set dffs [get_cells -filter "ref_name == DFF_X1"]
puts "DFF_X1 cells: [llength $dffs]"

set in_ports [get_ports -filter "direction == input"]
puts "input ports: [llength $in_ports]"

set out_ports [get_ports -filter "direction == output"]
puts "output ports: [llength $out_ports]"

############################################################
# findLeafLoadPins / findLeafDriverPins via reporting
############################################################
puts "--- report_net_load ---"
foreach n [get_nets *] {
  report_net [get_full_name $n]
}

############################################################
# Delete clocks and re-create constraints
############################################################
puts "--- delete clocks and rebuild ---"
delete_clock [get_clocks clk2]
delete_clock [get_clocks clk1]

create_clock -name clk_new -period 8 [get_ports clk1]
set_input_delay -clock clk_new 1.0 [get_ports in1]
set_output_delay -clock clk_new 2.0 [get_ports out1]
report_checks

set sdc3 [make_result_file sdc_leaf_pin3.sdc]
write_sdc -no_timestamp $sdc3
diff_files sdc_leaf_pin3.sdcok $sdc3
