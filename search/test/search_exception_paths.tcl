# Test exception paths: set_false_path, set_multicycle_path, set_max_delay,
# set_min_delay, set_data_check, and their effects on timing.
# Targets: Sta.cc makeFalsePath, makeMulticyclePath, makePathDelay,
#          resetPath, makeGroupPath, isGroupPathName, isPathGroupName,
#          pathGroupNames, makeExceptionFrom/To/Thru,
#          Search.cc arrivalsInvalid, endpointsInvalid,
#          PathEnd.cc PathEndPathDelay, PathEndCheck (multicycle),
#          VisitPathEnds.cc visitPathEnds with exceptions,
#          PathGroup.cc makeGroupPathEnds with exception paths
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_path_end_types.v
link_design search_path_end_types

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_input_delay -clock clk 0.5 [get_ports rst]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]
set_output_delay -clock clk 2.0 [get_ports out3]

# Baseline timing
report_checks -path_delay max > /dev/null

############################################################
# set_false_path from input to register
############################################################
puts "--- set_false_path -from port -to pin ---"
set_false_path -from [get_ports in1] -to [get_pins reg1/D]
report_checks -path_delay max -from [get_ports in1]

puts "--- remove false path ---"
unset_path_exceptions -from [get_ports in1] -to [get_pins reg1/D]
report_checks -path_delay max -from [get_ports in1]

############################################################
# set_false_path -through
############################################################
puts "--- set_false_path -through ---"
set_false_path -through [get_pins buf1/Z]
report_checks -path_delay max

puts "--- remove false_path through ---"
unset_path_exceptions -through [get_pins buf1/Z]
report_checks -path_delay max

############################################################
# set_false_path -setup / -hold
############################################################
puts "--- set_false_path -setup ---"
set_false_path -setup -from [get_ports in2] -to [get_pins reg1/D]
report_checks -path_delay max

puts "--- remove false_path setup ---"
unset_path_exceptions -setup -from [get_ports in2] -to [get_pins reg1/D]

puts "--- set_false_path -hold ---"
set_false_path -hold -from [get_ports in2] -to [get_pins reg1/D]
report_checks -path_delay min

puts "--- remove false_path hold ---"
unset_path_exceptions -hold -from [get_ports in2] -to [get_pins reg1/D]

############################################################
# set_multicycle_path setup and hold
############################################################
puts "--- set_multicycle_path 2 -setup ---"
set_multicycle_path 2 -setup -from [get_ports in1] -to [get_pins reg1/D]
report_checks -path_delay max -from [get_ports in1] -to [get_pins reg1/D]

puts "--- set_multicycle_path 1 -hold ---"
set_multicycle_path 1 -hold -from [get_ports in1] -to [get_pins reg1/D]
report_checks -path_delay min -from [get_ports in1] -to [get_pins reg1/D]

puts "--- set_multicycle_path 3 -setup with -through ---"
unset_path_exceptions -setup -from [get_ports in1] -to [get_pins reg1/D]
unset_path_exceptions -hold -from [get_ports in1] -to [get_pins reg1/D]
set_multicycle_path 3 -setup -through [get_pins and1/ZN]
report_checks -path_delay max -through [get_pins and1/ZN]

puts "--- remove multicycle through ---"
unset_path_exceptions -setup -through [get_pins and1/ZN]

############################################################
# set_max_delay / set_min_delay (PathDelay)
############################################################
puts "--- set_max_delay ---"
set_max_delay 5 -from [get_ports in1] -to [get_pins reg1/D]
report_checks -path_delay max -from [get_ports in1] -to [get_pins reg1/D]

puts "--- set_min_delay ---"
set_min_delay 0.1 -from [get_ports in1] -to [get_pins reg1/D]
report_checks -path_delay min -from [get_ports in1] -to [get_pins reg1/D]

puts "--- remove max/min delay ---"
unset_path_exceptions -from [get_ports in1] -to [get_pins reg1/D]
report_checks -path_delay max

############################################################
# set_max_delay -through (exercises PathEndPathDelay)
############################################################
puts "--- set_max_delay -through ---"
set_max_delay 8 -through [get_pins buf1/Z] -to [get_ports out1]
report_checks -path_delay max -through [get_pins buf1/Z]

puts "--- remove max_delay through ---"
unset_path_exceptions -through [get_pins buf1/Z] -to [get_ports out1]

############################################################
# group_path with various options
############################################################
puts "--- group_path -name from_in1 ---"
group_path -name from_in1 -from [get_ports in1]
report_checks -path_delay max

puts "--- group_path -name to_out1 ---"
group_path -name to_out1 -to [get_ports out1]
report_checks -path_delay max

puts "--- group_path -name through_buf ---"
group_path -name through_buf -through [get_pins buf1/Z]
report_checks -path_delay max

puts "--- report_checks -path_group ---"
report_checks -path_delay max -path_group from_in1
report_checks -path_delay max -path_group to_out1

puts "--- path_group_names ---"
set grp_names [sta::path_group_names]
puts "Path group names: $grp_names"

############################################################
# report_check_types with individual flags
############################################################
puts "--- report_check_types -max_delay ---"
report_check_types -max_delay

puts "--- report_check_types -min_delay ---"
report_check_types -min_delay

puts "--- report_check_types -recovery ---"
report_check_types -recovery

puts "--- report_check_types -removal ---"
report_check_types -removal

puts "--- report_check_types -max_delay -min_delay together ---"
report_check_types -max_delay -min_delay

puts "--- report_check_types -recovery -removal ---"
report_check_types -recovery -removal

puts "--- report_check_types -clock_gating_setup ---"
report_check_types -clock_gating_setup

puts "--- report_check_types -clock_gating_hold ---"
report_check_types -clock_gating_hold

puts "--- report_check_types -clock_gating_setup -clock_gating_hold ---"
report_check_types -clock_gating_setup -clock_gating_hold

puts "--- report_check_types -min_pulse_width ---"
report_check_types -min_pulse_width

puts "--- report_check_types -min_period ---"
report_check_types -min_period

puts "--- report_check_types -max_skew ---"
report_check_types -max_skew

puts "--- report_check_types -max_slew ---"
report_check_types -max_slew

puts "--- report_check_types -max_capacitance ---"
report_check_types -max_capacitance

puts "--- report_check_types -max_fanout ---"
report_check_types -max_fanout

puts "--- report_check_types -violators ---"
report_check_types -violators

puts "--- report_check_types -violators -verbose ---"
report_check_types -violators -verbose

############################################################
# worst_clock_skew
############################################################
puts "--- worst_clock_skew -setup ---"
set ws [worst_clock_skew -setup]
puts "worst_clock_skew setup: $ws"

puts "--- worst_clock_skew -hold ---"
set wh [worst_clock_skew -hold]
puts "worst_clock_skew hold: $wh"

############################################################
# total_negative_slack / worst_slack / worst_negative_slack
############################################################
puts "--- total_negative_slack -max ---"
set tns_max [total_negative_slack -max]
puts "tns max: $tns_max"

puts "--- total_negative_slack -min ---"
set tns_min [total_negative_slack -min]
puts "tns min: $tns_min"

puts "--- worst_slack -max ---"
set ws_max [worst_slack -max]
puts "worst_slack max: $ws_max"

puts "--- worst_slack -min ---"
set ws_min [worst_slack -min]
puts "worst_slack min: $ws_min"

puts "--- worst_negative_slack -max ---"
set wns_max [worst_negative_slack -max]
puts "wns max: $wns_max"

############################################################
# endpoint_slack via path group names
############################################################
puts "--- endpoint_slack ---"
set ep_pins [sta::endpoints]
foreach ep $ep_pins {
  set eslack [sta::endpoint_slack $ep "clk" max]
  puts "endpoint_slack [get_full_name $ep] clk max: $eslack"
  break
}

############################################################
# report_path with -min
############################################################
puts "--- report_path -min ---"
set pin_arg [get_pins reg1/D]
report_path -min $pin_arg rise

puts "--- report_path -max ---"
report_path -max $pin_arg fall

############################################################
# report_arrival / report_required / report_slack
############################################################
puts "--- report_arrival ---"
report_arrival [get_pins reg1/D]

puts "--- report_required ---"
report_required [get_pins reg1/D]

puts "--- report_slack ---"
report_slack [get_pins reg1/D]
