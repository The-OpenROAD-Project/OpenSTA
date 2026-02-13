# Test Sta.cc commands and various uncovered code paths
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

# Run timing
report_checks > /dev/null

puts "--- report_arrival on various pins ---"
report_arrival [get_pins reg1/D]
report_arrival [get_pins reg1/Q]
report_arrival [get_pins reg1/CK]
report_arrival [get_pins and1/ZN]
report_arrival [get_pins buf1/Z]
report_arrival [get_ports in1]
report_arrival [get_ports out1]
puts "PASS: report_arrival"

puts "--- report_required on various pins ---"
report_required [get_pins reg1/D]
report_required [get_ports out1]
puts "PASS: report_required"

puts "--- report_slack on various pins ---"
report_slack [get_pins reg1/D]
report_slack [get_ports out1]
puts "PASS: report_slack"

puts "--- worst_slack and TNS for each corner ---"
set ws_max [worst_slack -max]
set ws_min [worst_slack -min]
puts "Worst slack max: $ws_max"
puts "Worst slack min: $ws_min"

set tns_max [total_negative_slack -max]
set tns_min [total_negative_slack -min]
puts "TNS max: $tns_max"
puts "TNS min: $tns_min"

set wns_max [worst_negative_slack -max]
set wns_min [worst_negative_slack -min]
puts "WNS max: $wns_max"
puts "WNS min: $wns_min"
puts "PASS: worst slack and TNS"

puts "--- report_checks with set_max_delay path ---"
set_max_delay 5 -from [get_ports in1] -to [get_ports out1]
report_checks -path_delay max -from [get_ports in1] -to [get_ports out1]
report_checks -path_delay max -from [get_ports in1] -to [get_ports out1] -format full_clock
report_checks -path_delay max -from [get_ports in1] -to [get_ports out1] -format full_clock_expanded
unset_path_exceptions -from [get_ports in1] -to [get_ports out1]
puts "PASS: set_max_delay path"

puts "--- report_checks with set_min_delay path ---"
set_min_delay 0.1 -from [get_ports in1] -to [get_ports out1]
report_checks -path_delay min -from [get_ports in1] -to [get_ports out1]
report_checks -path_delay min -from [get_ports in1] -to [get_ports out1] -format full_clock
unset_path_exceptions -from [get_ports in1] -to [get_ports out1]
puts "PASS: set_min_delay path"

puts "--- report_checks with set_false_path ---"
set_false_path -from [get_ports in1] -to [get_pins reg1/D]
report_checks -path_delay max
unset_path_exceptions -from [get_ports in1] -to [get_pins reg1/D]
puts "PASS: set_false_path"

puts "--- report_checks with multicycle path ---"
set_multicycle_path 3 -setup -from [get_ports in1] -to [get_pins reg1/D]
set_multicycle_path 2 -hold -from [get_ports in1] -to [get_pins reg1/D]
report_checks -path_delay max -from [get_ports in1] -to [get_pins reg1/D] -format full_clock_expanded
report_checks -path_delay min -from [get_ports in1] -to [get_pins reg1/D] -format full_clock_expanded
unset_path_exceptions -setup -from [get_ports in1] -to [get_pins reg1/D]
unset_path_exceptions -hold -from [get_ports in1] -to [get_pins reg1/D]
puts "PASS: multicycle path"

puts "--- report_disabled_edges ---"
report_disabled_edges
set_disable_timing [get_cells buf1]
report_disabled_edges
unset_disable_timing [get_cells buf1]
puts "PASS: report_disabled_edges"

puts "--- report_constant ---"
set_case_analysis 1 [get_ports in2]
report_constant [get_ports in2]
report_constant [get_cells and1]
report_constant [get_pins and1/A2]
unset_case_analysis [get_ports in2]
puts "PASS: report_constant"

puts "--- set_clock_uncertainty setup/hold ---"
set_clock_uncertainty 0.2 -setup [get_clocks clk]
set_clock_uncertainty 0.1 -hold [get_clocks clk]
report_checks -path_delay max -format full_clock_expanded
report_checks -path_delay min -format full_clock_expanded
unset_clock_uncertainty -setup [get_clocks clk]
unset_clock_uncertainty -hold [get_clocks clk]
puts "PASS: clock_uncertainty setup/hold"

puts "--- set_clock_latency ---"
set_clock_latency 0.3 [get_clocks clk]
set_clock_latency -source 0.2 [get_clocks clk]
set_clock_latency -source -early 0.1 [get_clocks clk]
set_clock_latency -source -late 0.25 [get_clocks clk]
report_checks -path_delay max -format full_clock
report_checks -path_delay min -format full_clock
unset_clock_latency [get_clocks clk]
unset_clock_latency -source [get_clocks clk]
puts "PASS: clock_latency"

puts "--- timing derate ---"
set_timing_derate -early 0.95
set_timing_derate -late 1.05
report_checks -path_delay max
report_checks -path_delay min
unset_timing_derate
puts "PASS: timing derate"

puts "--- report_checks -format json ---"
report_checks -format json
puts "PASS: json format"

puts "--- report_checks -format summary ---"
report_checks -format summary -path_delay max
report_checks -format summary -path_delay min
puts "PASS: summary format"

puts "--- report_checks -format slack_only ---"
report_checks -format slack_only -path_delay max
report_checks -format slack_only -path_delay min
puts "PASS: slack_only format"

puts "--- report_checks -format end ---"
report_checks -format end -path_delay max
report_checks -format end -path_delay min
report_checks -format end -path_delay min_max
puts "PASS: end format"

puts "--- report_checks -format short ---"
report_checks -format short -path_delay max
report_checks -format short -path_delay min
puts "PASS: short format"

puts "--- pin_sim_logic_value ---"
set sim_val [sta::pin_sim_logic_value [get_pins and1/A1]]
puts "sim logic value: $sim_val"
puts "PASS: pin_sim_logic_value"

puts "--- worst_clk_skew ---"
set skew_setup [sta::worst_clk_skew_cmd setup 0]
set skew_hold [sta::worst_clk_skew_cmd hold 0]
puts "Worst clk skew setup: $skew_setup"
puts "Worst clk skew hold: $skew_hold"
set skew_setup_int [sta::worst_clk_skew_cmd setup 1]
set skew_hold_int [sta::worst_clk_skew_cmd hold 1]
puts "Worst clk skew setup (int): $skew_setup_int"
puts "Worst clk skew hold (int): $skew_hold_int"
puts "PASS: worst_clk_skew"

puts "--- report_clock_skew with include_internal_latency ---"
report_clock_skew -setup -include_internal_latency
report_clock_skew -hold -include_internal_latency
puts "PASS: clock_skew internal_latency"

puts "--- report_clock_latency with include_internal_latency ---"
report_clock_latency -include_internal_latency
puts "PASS: clock_latency internal_latency"

puts "ALL PASSED"
