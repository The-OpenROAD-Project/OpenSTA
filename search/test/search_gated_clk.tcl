# Test GatedClk.cc - clock gating related code paths
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_gated_clk.v
link_design search_gated_clk

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports en]
set_output_delay -clock clk 2.0 [get_ports out1]

puts "--- gated clk basic timing ---"
report_checks -path_delay max
report_checks -path_delay min

puts "--- gated_clk_checks_enabled ---"
sta::set_gated_clk_checks_enabled 1
report_checks -path_delay max
report_checks -path_delay min
report_check_types -verbose

puts "--- propagate_gated_clock_enable ---"
sta::set_propagate_gated_clock_enable 1
report_checks -path_delay max
report_checks -path_delay min
report_check_types -verbose
check_setup -verbose

puts "--- Gated clk with inferred clock gating ---"
set_disable_inferred_clock_gating [get_cells clk_gate]
report_checks -path_delay max
unset_disable_inferred_clock_gating [get_cells clk_gate]

sta::set_gated_clk_checks_enabled 0
sta::set_propagate_gated_clock_enable 0

puts "--- report_checks format full_clock with gated clk ---"
report_checks -format full_clock -path_delay max
report_checks -format full_clock_expanded -path_delay max
report_checks -format full_clock -path_delay min

puts "--- find_timing_paths with gated clk ---"
set paths [find_timing_paths -path_delay max -endpoint_count 5]
puts "Found [llength $paths] paths"
foreach pe $paths {
  puts "  is_gated_clock: [$pe is_gated_clock]"
  puts "  is_check: [$pe is_check]"
  puts "  pin: [get_full_name [$pe pin]]"
}
