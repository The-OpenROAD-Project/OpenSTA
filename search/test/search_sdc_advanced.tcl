# Test advanced SDC constraints that exercise search module code
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

puts "--- baseline report_checks ---"
report_checks -path_delay max

puts "--- set_clock_uncertainty ---"
set_clock_uncertainty 0.5 [get_clocks clk]
report_checks -path_delay max
puts "PASS: set_clock_uncertainty applied"

puts "--- unset_clock_uncertainty ---"
unset_clock_uncertainty [get_clocks clk]

puts "--- set_clock_latency -source ---"
set_clock_latency -source 0.2 [get_clocks clk]
report_checks -path_delay max
puts "PASS: set_clock_latency -source applied"

puts "--- unset_clock_latency -source ---"
unset_clock_latency -source [get_clocks clk]

puts "--- set_clock_latency (network) ---"
set_clock_latency 0.1 [get_clocks clk]
report_checks -path_delay max
puts "PASS: set_clock_latency applied"

puts "--- unset_clock_latency ---"
unset_clock_latency [get_clocks clk]

puts "--- set_timing_derate -early ---"
set_timing_derate -early 0.95
report_checks -path_delay min
puts "PASS: set_timing_derate -early applied"

puts "--- set_timing_derate -late ---"
set_timing_derate -late 1.05
report_checks -path_delay max
puts "PASS: set_timing_derate -late applied"

puts "--- unset_timing_derate ---"
unset_timing_derate
report_checks -path_delay max
puts "PASS: unset_timing_derate applied"

puts "--- set_case_analysis on port ---"
set_case_analysis 1 [get_ports in2]
report_checks -path_delay max
report_constant [get_ports in2]
puts "PASS: set_case_analysis applied"

puts "--- unset_case_analysis ---"
unset_case_analysis [get_ports in2]
report_checks -path_delay max
puts "PASS: unset_case_analysis applied"

puts "--- set_disable_timing on cell ---"
set_disable_timing [get_cells buf1]
report_checks -path_delay max
report_disabled_edges
puts "PASS: set_disable_timing applied"

puts "--- unset_disable_timing ---"
unset_disable_timing [get_cells buf1]
report_checks -path_delay max
puts "PASS: unset_disable_timing applied"

puts "--- set_disable_timing with from/to on lib cell ---"
set_disable_timing -from A -to Z [get_lib_cells Nangate45_typ/BUF_X1]
report_checks -path_delay max
report_disabled_edges
puts "PASS: set_disable_timing from/to on lib cell applied"

puts "--- unset lib cell disable ---"
unset_disable_timing -from A -to Z [get_lib_cells Nangate45_typ/BUF_X1]
report_checks -path_delay max
puts "PASS: unset lib cell disable_timing applied"

puts "--- set_max_delay ---"
set_max_delay 5 -from [get_ports in1] -to [get_pins reg1/D]
report_checks -path_delay max -from [get_ports in1] -to [get_pins reg1/D]
puts "PASS: set_max_delay applied"

puts "--- remove max delay via unset_path_exceptions ---"
unset_path_exceptions -from [get_ports in1] -to [get_pins reg1/D]

puts "--- set_min_delay ---"
set_min_delay 0.5 -from [get_ports in1] -to [get_pins reg1/D]
report_checks -path_delay min -from [get_ports in1] -to [get_pins reg1/D]
puts "PASS: set_min_delay applied"

puts "--- remove min delay via unset_path_exceptions ---"
unset_path_exceptions -from [get_ports in1] -to [get_pins reg1/D]
puts "PASS: removed delay constraints"

puts "--- set_false_path from/to ---"
set_false_path -from [get_ports in1] -to [get_pins reg1/D]
report_checks -path_delay max
puts "PASS: set_false_path applied"

puts "--- remove false path via unset_path_exceptions ---"
unset_path_exceptions -from [get_ports in1] -to [get_pins reg1/D]
report_checks -path_delay max
puts "PASS: removed false path"

puts "--- set_false_path -through ---"
set_false_path -through [get_pins buf1/Z]
report_checks -path_delay max
puts "PASS: set_false_path -through applied"

puts "--- remove false_path -through ---"
unset_path_exceptions -through [get_pins buf1/Z]
report_checks -path_delay max
puts "PASS: removed false path -through"

puts "--- set_multicycle_path -setup ---"
set_multicycle_path 2 -setup -from [get_ports in1] -to [get_pins reg1/D]
report_checks -path_delay max -from [get_ports in1] -to [get_pins reg1/D]
puts "PASS: set_multicycle_path -setup applied"

puts "--- set_multicycle_path -hold ---"
set_multicycle_path 1 -hold -from [get_ports in1] -to [get_pins reg1/D]
report_checks -path_delay min -from [get_ports in1] -to [get_pins reg1/D]
puts "PASS: set_multicycle_path -hold applied"

puts "--- remove multicycle paths ---"
unset_path_exceptions -setup -from [get_ports in1] -to [get_pins reg1/D]
unset_path_exceptions -hold -from [get_ports in1] -to [get_pins reg1/D]
puts "PASS: removed multicycle paths"

puts "--- group_path ---"
group_path -name fast_group -from [get_ports in1]
report_checks -path_delay max
puts "PASS: group_path applied"

puts "--- group_path with -to ---"
group_path -name out_group -to [get_ports out1]
report_checks -path_delay max
puts "PASS: group_path -to applied"

puts "--- report_check_types after constraints ---"
report_check_types -verbose

puts "--- check_setup after all constraints ---"
check_setup -verbose

puts "ALL SDC advanced tests PASSED"
