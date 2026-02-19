# Test Latches.cc - latch timing analysis
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_latch.v
link_design search_latch

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]

puts "--- report_checks max through latch ---"
report_checks -path_delay max

puts "--- report_checks min through latch ---"
report_checks -path_delay min

puts "--- report_checks min_max ---"
report_checks -path_delay min_max

puts "--- report_checks full_clock through latch ---"
report_checks -format full_clock -path_delay max

puts "--- report_checks full_clock_expanded through latch ---"
report_checks -format full_clock_expanded -path_delay max

puts "--- report_checks to latch output ---"
report_checks -to [get_pins latch1/Q] -path_delay max

puts "--- report_checks from latch output ---"
report_checks -from [get_pins latch1/Q] -path_delay max

puts "--- report_check_types with latch ---"
report_check_types -verbose

puts "--- check_setup with latch ---"
check_setup -verbose

puts "--- report_clock_skew ---"
report_clock_skew -setup
report_clock_skew -hold

puts "--- report_clock_latency ---"
report_clock_latency

puts "--- report_pulse_width_checks ---"
report_pulse_width_checks -verbose

puts "--- find_timing_paths through latch ---"
set paths [find_timing_paths -path_delay max -endpoint_path_count 5]
puts "Found [llength $paths] paths"
foreach pe $paths {
  puts "  path to [get_full_name [$pe pin]] slack=[$pe slack]"
}

puts "--- all_registers with latch ---"
set reg_cells [all_registers -cells]
puts "Register cells: [llength $reg_cells]"
foreach cell $reg_cells {
  puts "  [get_full_name $cell]"
}

set reg_cells_lt [all_registers -cells -level_sensitive]
puts "Level-sensitive cells: [llength $reg_cells_lt]"

set reg_cells_et [all_registers -cells -edge_triggered]
puts "Edge-triggered cells: [llength $reg_cells_et]"

set data_pins [all_registers -data_pins]
puts "Data pins: [llength $data_pins]"

set clk_pins [all_registers -clock_pins]
puts "Clock pins: [llength $clk_pins]"

set out_pins [all_registers -output_pins]
puts "Output pins: [llength $out_pins]"

puts "--- report_tns/report_wns with latch ---"
report_tns -max
report_wns -max
report_worst_slack -max
report_worst_slack -min
