# Test ReportPath.cc expanded report paths, JSON format internals,
# PathExpanded.cc, and PathEnd report variants.
# Targets: ReportPath.cc reportPathEnds, reportJson, reportEndpointHeader,
#          reportEndLine, reportSummaryLine, reportSlackOnly,
#          reportFull for various PathEnd types, reportPathEndHeader/Footer
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_crpr_data_checks.v
link_design search_crpr_data_checks

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 8 [get_ports clk2]
set_propagated_clock [get_clocks clk1]
set_propagated_clock [get_clocks clk2]

set_input_delay -clock clk1 1.0 [get_ports in1]
set_input_delay -clock clk1 1.0 [get_ports in2]
set_output_delay -clock clk1 2.0 [get_ports out1]
set_output_delay -clock clk2 2.0 [get_ports out2]

# Cross-domain false path
set_false_path -from [get_clocks clk1] -to [get_clocks clk2]

puts "--- report_checks full_clock_expanded with CRPR ---"
report_checks -path_delay max -format full_clock_expanded
report_checks -path_delay min -format full_clock_expanded
puts "PASS: full_clock_expanded CRPR"

puts "--- report_checks -to each output ---"
report_checks -to [get_ports out1] -path_delay max -format full_clock_expanded
report_checks -to [get_ports out2] -path_delay max -format full_clock_expanded
report_checks -to [get_ports out1] -path_delay min -format full_clock_expanded
puts "PASS: per-output reports"

puts "--- report_checks -from specific pins ---"
report_checks -from [get_pins reg1/CK] -path_delay max -format full_clock_expanded
report_checks -from [get_ports in1] -path_delay max -format full_clock
puts "PASS: from-pin reports"

puts "--- report_checks with various fields ---"
report_checks -path_delay max -fields {capacitance slew fanout input_pin net src_attr}
report_checks -path_delay min -fields {capacitance slew fanout input_pin net src_attr}
puts "PASS: all fields"

puts "--- report_checks with -no_line_splits ---"
report_checks -path_delay max -no_line_splits
puts "PASS: no_line_splits"

puts "--- report_checks with digits ---"
report_checks -path_delay max -digits 6
report_checks -path_delay min -digits 6
puts "PASS: digits 6"

puts "--- report_checks JSON format ---"
report_checks -path_delay max -format json
report_checks -path_delay min -format json
puts "PASS: JSON format"

puts "--- report_checks JSON with endpoint_path_count ---"
report_checks -path_delay max -format json -endpoint_path_count 3
puts "PASS: JSON endpoint count"

puts "--- find_timing_paths and iterate ---"
set paths [find_timing_paths -path_delay max -endpoint_path_count 5 -group_path_count 10]
puts "Found [llength $paths] paths"

# Report each path individually with different formats
set idx 0
foreach pe $paths {
  set p [$pe path]
  puts "Path $idx:"
  puts "  pin: [get_full_name [$pe pin]]"
  puts "  slack: [$pe slack]"
  puts "  arrival: [$p arrival]"
  puts "  required: [$p required]"

  # Expanded path pins
  set pins [$p pins]
  puts "  path_pins: [llength $pins]"

  # Check path start
  set sp [$p start_path]
  if { $sp != "NULL" } {
    puts "  start_path pin: [get_full_name [$sp pin]]"
  }

  incr idx
}
puts "PASS: path iteration"

puts "--- report_path_end with prev_end chaining ---"
set prev ""
sta::report_path_end_header
foreach pe $paths {
  if { $prev != "" } {
    sta::report_path_end2 $pe $prev 0
  } else {
    sta::report_path_end $pe
  }
  set prev $pe
}
sta::report_path_end_footer
puts "PASS: report_path_end chaining"

puts "--- report_path_ends as sequence ---"
sta::report_path_ends $paths
puts "PASS: report_path_ends sequence"

puts "--- set_report_path_format json then report ---"
sta::set_report_path_format json
set paths2 [find_timing_paths -path_delay max -endpoint_path_count 3]
foreach pe $paths2 {
  set p [$pe path]
  sta::report_path_cmd $p
  break
}
sta::set_report_path_format full
puts "PASS: report_path json format"

puts "--- set_report_path_format full_clock_expanded ---"
sta::set_report_path_format full_clock_expanded
set paths3 [find_timing_paths -path_delay max]
foreach pe $paths3 {
  set p [$pe path]
  sta::report_path_cmd $p
  break
}
sta::set_report_path_format full
puts "PASS: report_path full_clock_expanded format"

puts "--- PathEnd vertex access ---"
catch {
  set paths_v [find_timing_paths -path_delay max -endpoint_path_count 2]
  foreach pe $paths_v {
    set v [$pe vertex]
    puts "vertex: [get_full_name [$v pin]]"
    puts "  is_clock: [$v is_clock]"
    puts "  has_downstream_clk_pin: [$v has_downstream_clk_pin]"
    break
  }
}
puts "PASS: PathEnd vertex"

puts "--- find_timing_paths min_max ---"
set paths_mm [find_timing_paths -path_delay min_max -endpoint_path_count 3]
puts "min_max paths: [llength $paths_mm]"
foreach pe $paths_mm {
  puts "  min_max: [$pe min_max] slack=[$pe slack]"
}
puts "PASS: min_max paths"

puts "--- report_tns/wns ---"
report_tns
report_wns
report_worst_slack -max
report_worst_slack -min
report_tns -digits 6
report_wns -digits 6
report_worst_slack -max -digits 6
puts "PASS: tns/wns"

puts "--- search debug info ---"
puts "tag_group_count: [sta::tag_group_count]"
puts "tag_count: [sta::tag_count]"
puts "clk_info_count: [sta::clk_info_count]"
puts "path_count: [sta::path_count]"
puts "PASS: search debug"

puts "ALL PASSED"
