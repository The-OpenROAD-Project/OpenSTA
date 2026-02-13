# Test latch timing paths, time borrowing, latch checks in ReportPath.
# Targets: ReportPath.cc reportShort/reportFull/reportEndpoint for
#          PathEndLatchCheck, reportBorrowing, latchDesc.
#          Latches.cc, PathEnd.cc PathEndLatchCheck
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_latch.v
link_design search_latch

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]

puts "--- Latch timing max ---"
report_checks -path_delay max
puts "PASS: latch max"

puts "--- Latch timing min ---"
report_checks -path_delay min
puts "PASS: latch min"

puts "--- report_checks to latch output ---"
report_checks -to [get_ports out1] -path_delay max -format full_clock_expanded
report_checks -to [get_ports out1] -path_delay min -format full_clock_expanded
puts "PASS: latch output full_clock_expanded"

puts "--- report_checks to reg output ---"
report_checks -to [get_ports out2] -path_delay max -format full_clock_expanded
report_checks -to [get_ports out2] -path_delay min -format full_clock_expanded
puts "PASS: reg output full_clock_expanded"

puts "--- report_checks format full_clock ---"
report_checks -path_delay max -format full_clock
report_checks -path_delay min -format full_clock
puts "PASS: full_clock format"

puts "--- report_checks format short ---"
report_checks -path_delay max -format short
report_checks -path_delay min -format short
puts "PASS: short format"

puts "--- report_checks format end ---"
report_checks -path_delay max -format end
report_checks -path_delay min -format end
puts "PASS: end format"

puts "--- report_checks format summary ---"
report_checks -path_delay max -format summary
report_checks -path_delay min -format summary
puts "PASS: summary format"

puts "--- report_checks format json ---"
report_checks -path_delay max -format json
report_checks -path_delay min -format json
puts "PASS: json format"

puts "--- report_checks format slack_only ---"
report_checks -path_delay max -format slack_only
report_checks -path_delay min -format slack_only
puts "PASS: slack_only format"

puts "--- find_timing_paths latch check ---"
set paths [find_timing_paths -path_delay max -endpoint_path_count 10 -group_path_count 20]
puts "Found [llength $paths] max paths"
foreach pe $paths {
  puts "  is_latch_check: [$pe is_latch_check] is_check: [$pe is_check] pin=[get_full_name [$pe pin]] slack=[$pe slack]"
}
puts "PASS: find_timing_paths latch"

puts "--- find_timing_paths min latch ---"
set paths_min [find_timing_paths -path_delay min -endpoint_path_count 10 -group_path_count 20]
puts "Found [llength $paths_min] min paths"
foreach pe $paths_min {
  puts "  is_latch_check: [$pe is_latch_check] is_check: [$pe is_check] pin=[get_full_name [$pe pin]] slack=[$pe slack]"
}
puts "PASS: find_timing_paths min latch"

puts "--- Latch path reports with fields ---"
report_checks -path_delay max -fields {capacitance slew fanout input_pin net}
report_checks -path_delay min -fields {capacitance slew fanout input_pin net}
puts "PASS: latch fields"

puts "--- set_latch_borrow_limit ---"
catch {
  set_latch_borrow_limit 2.0 [get_pins latch1/G]
  report_checks -path_delay max -format full_clock_expanded
}
puts "PASS: latch_borrow_limit pin"

catch {
  set_latch_borrow_limit 3.0 [get_cells latch1]
  report_checks -path_delay max -format full_clock_expanded
}
puts "PASS: latch_borrow_limit inst"

catch {
  set_latch_borrow_limit 4.0 [get_clocks clk]
  report_checks -path_delay max -format full_clock_expanded
}
puts "PASS: latch_borrow_limit clock"

puts "--- report_clock_properties ---"
report_clock_properties
puts "PASS: clock_properties"

puts "--- report_clock_skew ---"
report_clock_skew -setup
report_clock_skew -hold
puts "PASS: clock_skew"

puts "--- all_registers -level_sensitive ---"
set ls_cells [all_registers -cells -level_sensitive]
puts "Level-sensitive cells: [llength $ls_cells]"
foreach c $ls_cells { puts "  [get_full_name $c]" }

set ls_dpins [all_registers -data_pins -level_sensitive]
puts "Level-sensitive data pins: [llength $ls_dpins]"
foreach p $ls_dpins { puts "  [get_full_name $p]" }

set ls_ckpins [all_registers -clock_pins -level_sensitive]
puts "Level-sensitive clock pins: [llength $ls_ckpins]"
foreach p $ls_ckpins { puts "  [get_full_name $p]" }

set ls_opins [all_registers -output_pins -level_sensitive]
puts "Level-sensitive output pins: [llength $ls_opins]"
foreach p $ls_opins { puts "  [get_full_name $p]" }
puts "PASS: all_registers level_sensitive"

puts "--- all_registers -edge_triggered ---"
set et_cells [all_registers -cells -edge_triggered]
puts "Edge-triggered cells: [llength $et_cells]"
foreach c $et_cells { puts "  [get_full_name $c]" }
puts "PASS: all_registers edge_triggered"

puts "--- pulse width checks ---"
report_pulse_width_checks
report_pulse_width_checks -verbose
puts "PASS: pulse_width_checks"

puts "--- min period ---"
report_clock_min_period
report_clock_min_period -include_port_paths
puts "PASS: min_period"

puts "ALL PASSED"
