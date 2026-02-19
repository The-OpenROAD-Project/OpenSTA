# Test Crpr.cc - Clock Reconvergence Pessimism Removal
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_crpr.v
link_design search_crpr

create_clock -name clk -period 10 [get_ports clk]
set_propagated_clock [get_clocks clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

# Add clock uncertainty to trigger CRPR paths
set_clock_uncertainty 0.5 -setup [get_clocks clk]
set_clock_uncertainty 0.3 -hold [get_clocks clk]

# Add timing derate for early/late
set_timing_derate -early 0.95
set_timing_derate -late 1.05

puts "--- CRPR with propagated clock, setup ---"
report_checks -path_delay max -format full_clock_expanded

puts "--- CRPR with propagated clock, hold ---"
report_checks -path_delay min -format full_clock_expanded

puts "--- report_checks full_clock ---"
report_checks -path_delay max -format full_clock

puts "--- report_checks between reg1 and reg2 ---"
report_checks -from [get_pins reg1/CK] -to [get_pins reg2/D] -path_delay max -format full_clock_expanded

puts "--- report_checks min between reg1 and reg2 ---"
report_checks -from [get_pins reg1/CK] -to [get_pins reg2/D] -path_delay min -format full_clock_expanded

puts "--- report_clock_skew ---"
report_clock_skew -setup
report_clock_skew -hold

puts "--- report_clock_latency ---"
report_clock_latency

puts "--- check CRPR mode settings ---"
puts "CRPR enabled: [sta::crpr_enabled]"
sta::set_crpr_enabled 1
puts "CRPR mode: [sta::crpr_mode]"
sta::set_crpr_mode "same_pin"
puts "CRPR mode after set: [sta::crpr_mode]"
report_checks -path_delay max -format full_clock_expanded
sta::set_crpr_mode "same_transition"
puts "CRPR mode after set: [sta::crpr_mode]"
report_checks -path_delay max -format full_clock_expanded

puts "--- CRPR disabled ---"
sta::set_crpr_enabled 0
report_checks -path_delay max -format full_clock_expanded
sta::set_crpr_enabled 1

puts "--- find_timing_paths with CRPR ---"
set paths [find_timing_paths -path_delay max -endpoint_path_count 3]
puts "Found [llength $paths] paths with CRPR"
foreach pe $paths {
  puts "  slack=[$pe slack] crpr=[$pe check_crpr]"
}

puts "--- find_timing_paths min with CRPR ---"
set paths_min [find_timing_paths -path_delay min -endpoint_path_count 3]
puts "Found [llength $paths_min] hold paths with CRPR"
foreach pe $paths_min {
  puts "  slack=[$pe slack] crpr=[$pe check_crpr]"
}

puts "--- report_check_types ---"
report_check_types -verbose
