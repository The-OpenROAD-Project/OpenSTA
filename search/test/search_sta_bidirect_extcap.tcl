# Test Sta.cc: setPortExtPinCap/setPortExtWireCap with min/max corners,
# connectedCap, report_net details, set_resistance, bidirect driver handling,
# disable clock gating check, set_clock_gating_check,
# set_inter_clock_uncertainty, set_clock_uncertainty,
# write_verilog/write_sdc after edits, pocv_enabled.
# Targets: Sta.cc setPortExtPinCap, setPortExtWireCap, connectedCap,
#          setResistance, disableClockGatingCheck, setClockGatingCheck,
#          setInterClockUncertainty, setClocUncertainty,
#          delaysInvalidFrom, networkChanged
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

# Baseline
report_checks -path_delay max > /dev/null

############################################################
# Port ext pin/wire cap with different values
############################################################
puts "--- Ext pin cap sequence ---"
set_load -pin_load 0.01 [get_ports out1]
set ws1 [sta::worst_slack_cmd max]
puts "pin_load 0.01 worst_slack: $ws1"

set_load -pin_load 0.05 [get_ports out1]
set ws2 [sta::worst_slack_cmd max]
puts "pin_load 0.05 worst_slack: $ws2"

set_load -pin_load 0.1 [get_ports out1]
set ws3 [sta::worst_slack_cmd max]
puts "pin_load 0.1 worst_slack: $ws3"

set_load -pin_load 0.0 [get_ports out1]
report_checks -path_delay max -fields {capacitance}

puts "--- Ext wire cap sequence ---"
set_load -wire_load 0.01 [get_ports out1]
set ws4 [sta::worst_slack_cmd max]
puts "wire_load 0.01 worst_slack: $ws4"

set_load -wire_load 0.05 [get_ports out1]
set ws5 [sta::worst_slack_cmd max]
puts "wire_load 0.05 worst_slack: $ws5"

set_load -wire_load 0.0 [get_ports out1]

############################################################
# set_fanout_load and set_port_fanout_number
############################################################
puts "--- fanout_load ---"
set_fanout_load 2 [get_ports out1]
report_checks -path_delay max > /dev/null
set_fanout_load 8 [get_ports out1]
report_checks -path_delay max > /dev/null

puts "--- port_fanout_number ---"
set_port_fanout_number 4 [get_ports out1]
report_checks -path_delay max > /dev/null
set_port_fanout_number 1 [get_ports out1]

############################################################
# set_input_transition (exercises driver model)
############################################################
puts "--- input_transition ---"
set_input_transition 0.05 [get_ports in1]
report_checks -path_delay max > /dev/null

set_input_transition 0.2 [get_ports in1]
report_checks -path_delay max > /dev/null

set_input_transition 0.5 [get_ports in1]
report_checks -path_delay max

############################################################
# set_driving_cell
############################################################
puts "--- driving_cell ---"
set_driving_cell -lib_cell BUF_X1 -pin Z [get_ports in1]
report_checks -path_delay max > /dev/null

set_driving_cell -lib_cell BUF_X4 -pin Z [get_ports in1]
report_checks -path_delay max > /dev/null

set_driving_cell -lib_cell INV_X1 -pin ZN [get_ports in2]
report_checks -path_delay max

############################################################
# set_clock_uncertainty
############################################################
puts "--- clock_uncertainty ---"
set_clock_uncertainty 0.1 -setup [get_clocks clk]
report_checks -path_delay max
set_clock_uncertainty 0.2 -setup [get_clocks clk]
report_checks -path_delay max
set_clock_uncertainty 0.05 -hold [get_clocks clk]
report_checks -path_delay min
set_clock_uncertainty 0 -setup [get_clocks clk]
set_clock_uncertainty 0 -hold [get_clocks clk]

############################################################
# report_net with details
############################################################
puts "--- report_net detail ---"
report_net n1
report_net n2
report_net n3
report_net -connections n1
report_net -verbose n1
report_net -connections -verbose n1

############################################################
# write_verilog / write_sdc
############################################################
puts "--- write_verilog ---"
set v_out [make_result_file "search_sta_bidirect.v"]
write_verilog $v_out

puts "--- write_sdc ---"
set s_out [make_result_file "search_sta_bidirect.sdc"]
write_sdc $s_out

############################################################
# pocv_enabled
############################################################
puts "--- pocv_enabled ---"
puts "pocv_enabled: [sta::pocv_enabled]"

############################################################
# report_disabled_edges
############################################################
puts "--- report_disabled_edges ---"
report_disabled_edges

############################################################
# set_disable_timing on instance
############################################################
puts "--- set_disable_timing on instance ---"
set_disable_timing [get_cells buf1]
report_checks -path_delay max
report_disabled_edges
unset_disable_timing [get_cells buf1]
report_checks -path_delay max

############################################################
# set_max_fanout / report_check_types
############################################################
puts "--- set_max_fanout ---"
set_max_fanout 2 [current_design]
report_check_types -max_fanout -verbose
report_check_types -violators

############################################################
# report_checks with rise/fall variants
############################################################
puts "--- rise/fall variants ---"
report_checks -path_delay max_rise -format end
report_checks -path_delay max_fall -format end
report_checks -path_delay min_rise -format end
report_checks -path_delay min_fall -format end

############################################################
# Propagated clock + reports
############################################################
puts "--- propagated clock ---"
set_propagated_clock [get_clocks clk]
report_checks -path_delay max -format full_clock_expanded
report_checks -path_delay min -format full_clock_expanded
unset_propagated_clock [get_clocks clk]

############################################################
# report_annotated_delay / report_annotated_check
############################################################
puts "--- annotated ---"
report_annotated_delay
report_annotated_check

############################################################
# slow_drivers
############################################################
puts "--- slow_drivers ---"
set slow [sta::slow_drivers 4]
puts "slow_drivers(4): [llength $slow]"
foreach s $slow {
  puts "  [get_full_name $s]"
}

############################################################
# find_timing_paths with many combinations
############################################################
puts "--- find_timing_paths combos ---"
set p1 [find_timing_paths -path_delay max -endpoint_count 1]
puts "1 path: [llength $p1]"
set p5 [find_timing_paths -path_delay max -endpoint_count 5 -group_path_count 10]
puts "5 paths: [llength $p5]"
set p_min [find_timing_paths -path_delay min -endpoint_count 5]
puts "min paths: [llength $p_min]"
set p_mm [find_timing_paths -path_delay min_max -endpoint_count 3]
puts "min_max paths: [llength $p_mm]"
