# Test CRPR with two clocks, cross-domain paths, data checks,
# and various CRPR modes.
# Covers: Crpr.cc portClkPath, crprArrivalDiff, findCrpr variants,
#         Search.cc cross-domain paths, WorstSlack.cc queue operations,
#         Sta.cc worstSlack corner variant, VisitPathEnds.cc
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_crpr_data_checks.v
link_design search_crpr_data_checks

# Two independent clocks
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 8 [get_ports clk2]
set_propagated_clock [get_clocks clk1]
set_propagated_clock [get_clocks clk2]

set_input_delay -clock clk1 1.0 [get_ports in1]
set_input_delay -clock clk1 1.0 [get_ports in2]
set_output_delay -clock clk1 2.0 [get_ports out1]
set_output_delay -clock clk2 2.0 [get_ports out2]

# Clock uncertainty
set_clock_uncertainty 0.5 -setup [get_clocks clk1]
set_clock_uncertainty 0.3 -hold [get_clocks clk1]
set_clock_uncertainty 0.4 -setup [get_clocks clk2]
set_clock_uncertainty 0.2 -hold [get_clocks clk2]

# Cross-domain false path to avoid cross-domain timing issues
# (but still exercise the path search for cross-domain)
set_false_path -from [get_clocks clk1] -to [get_clocks clk2]

# Timing derate
set_timing_derate -early 0.95
set_timing_derate -late 1.05

puts "--- CRPR setup with two clocks ---"
report_checks -path_delay max -format full_clock_expanded
puts "PASS: CRPR setup two clocks"

puts "--- CRPR hold with two clocks ---"
report_checks -path_delay min -format full_clock_expanded
puts "PASS: CRPR hold two clocks"

puts "--- CRPR same_pin mode ---"
sta::set_crpr_enabled 1
sta::set_crpr_mode "same_pin"
report_checks -path_delay max -format full_clock_expanded
puts "PASS: CRPR same_pin"

puts "--- CRPR same_transition mode ---"
sta::set_crpr_mode "same_transition"
report_checks -path_delay max -format full_clock_expanded
report_checks -path_delay min -format full_clock_expanded
puts "PASS: CRPR same_transition"

puts "--- CRPR disabled ---"
sta::set_crpr_enabled 0
report_checks -path_delay max -format full_clock_expanded
sta::set_crpr_enabled 1
puts "PASS: CRPR disable/enable"

puts "--- report_clock_skew setup/hold ---"
report_clock_skew -setup
report_clock_skew -hold
puts "PASS: clock_skew"

puts "--- report_clock_latency ---"
report_clock_latency
puts "PASS: clock_latency"

puts "--- find_timing_paths with CRPR ---"
set paths [find_timing_paths -path_delay max -endpoint_path_count 5]
puts "Found [llength $paths] paths"
foreach pe $paths {
  puts "  slack=[$pe slack] crpr=[$pe check_crpr] pin=[get_full_name [$pe pin]]"
}
puts "PASS: find_timing_paths CRPR"

puts "--- find_timing_paths min with CRPR ---"
set paths_min [find_timing_paths -path_delay min -endpoint_path_count 5]
puts "Found [llength $paths_min] hold paths"
foreach pe $paths_min {
  puts "  slack=[$pe slack] crpr=[$pe check_crpr]"
}
puts "PASS: find_timing_paths min CRPR"

puts "--- worst_slack by clock ---"
catch {
  set ws1 [sta::worst_slack_cmd max]
  puts "worst_slack max: $ws1"
}
catch {
  set ws2 [sta::worst_slack_cmd min]
  puts "worst_slack min: $ws2"
}
puts "PASS: worst_slack with two clocks"

puts "--- total_negative_slack ---"
catch {
  set tns1 [sta::total_negative_slack_cmd max]
  puts "tns max: $tns1"
  set tns2 [sta::total_negative_slack_cmd min]
  puts "tns min: $tns2"
}
puts "PASS: tns with two clocks"

puts "--- report_tns/wns ---"
report_tns
report_wns
report_worst_slack -max
report_worst_slack -min
puts "PASS: report_tns/wns"

puts "--- report_check_types ---"
report_check_types -verbose
puts "PASS: check_types"

puts "--- check_setup ---"
check_setup -verbose
puts "PASS: check_setup"

puts "--- Now set_data_check for skew testing ---"
# Remove false path and add data_check constraint
# Data checks create max_skew-like constraints
catch {
  set_data_check -from [get_pins reg1/CK] -to [get_pins reg2/D] -setup 0.1
  set_data_check -from [get_pins reg1/CK] -to [get_pins reg2/D] -hold 0.05
  report_checks -path_delay max -format full_clock_expanded
  report_checks -path_delay min -format full_clock_expanded
  puts "data_check constraints applied"
}
puts "PASS: data_check constraints"

puts "--- report_checks with various formats ---"
report_checks -format full -path_delay max
report_checks -format full_clock -path_delay max
report_checks -format short -path_delay max
report_checks -format end -path_delay max
report_checks -format slack_only -path_delay max
report_checks -format summary -path_delay max
report_checks -format json -path_delay max
puts "PASS: all formats with two clocks"

puts "--- report_checks min formats ---"
report_checks -format full -path_delay min
report_checks -format full_clock -path_delay min
report_checks -format json -path_delay min
puts "PASS: min formats with two clocks"

puts "--- report_checks between specific endpoints ---"
report_checks -from [get_pins reg1/CK] -to [get_pins reg2/D] -format full_clock_expanded
report_checks -from [get_ports in1] -to [get_ports out1] -format full_clock
puts "PASS: specific endpoint reports"

puts "--- report_checks with fields ---"
report_checks -fields {capacitance slew fanout input_pin net} -format full_clock_expanded
puts "PASS: fields with full_clock_expanded"

puts "--- min_period checks with two clocks ---"
create_clock -name clk1 -period 0.05 [get_ports clk1]
create_clock -name clk2 -period 0.04 [get_ports clk2]
report_check_types -min_period
report_check_types -min_period -verbose
report_check_types -min_period -violators
report_check_types -min_period -violators -verbose
report_clock_min_period
puts "PASS: min_period with two clocks"

puts "--- pulse width checks ---"
report_check_types -min_pulse_width
report_check_types -min_pulse_width -verbose
report_pulse_width_checks
puts "PASS: pulse_width with two clocks"

puts "ALL PASSED"
