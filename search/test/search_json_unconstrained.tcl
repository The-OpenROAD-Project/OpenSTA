# Test ReportPath::reportJson with multiple paths, reportJsonHeader,
# reportFull(PathEndUnconstrained), reportShort(PathEndUnconstrained),
# and report_checks -format json with various path types.
# Covers: ReportPath.cc reportJson*, reportFull/Short(Unconstrained),
#         reportPath(PathEnd, PathExpanded), reportBlankLine, reportClkPath
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

# Force timing computation
report_checks > /dev/null

puts "--- report_checks -format json (multiple endpoints) ---"
report_checks -format json -path_delay max -endpoint_path_count 3 -group_path_count 5

puts "--- report_checks -format json min ---"
report_checks -format json -path_delay min

puts "--- report_checks -format json min_max ---"
report_checks -format json -path_delay min_max

puts "--- report_checks -format json to specific pin ---"
report_checks -format json -to [get_pins reg1/D]

puts "--- report_checks -format json from specific port ---"
report_checks -format json -from [get_ports in1]

puts "--- report_checks -format json through pin ---"
report_checks -format json -through [get_pins and1/ZN]

puts "--- report_checks -unconstrained format full ---"
report_checks -unconstrained -format full

puts "--- report_checks -unconstrained format short ---"
report_checks -unconstrained -format short

puts "--- report_checks -unconstrained format end ---"
report_checks -unconstrained -format end

puts "--- report_checks -unconstrained format slack_only ---"
report_checks -unconstrained -format slack_only

puts "--- report_path on individual path (json format) ---"
sta::set_report_path_format json
set paths_j [find_timing_paths -path_delay max -endpoint_path_count 3]
foreach pe $paths_j {
  set p [$pe path]
  sta::report_path_cmd $p
}
sta::set_report_path_format full

puts "--- reportPathFull on a single path ---"
set paths_f [find_timing_paths -path_delay max]
foreach pe $paths_f {
  set p [$pe path]
  sta::report_path_cmd $p
  break
}

puts "--- json report with full_clock format (for source clock paths) ---"
report_checks -format json -path_delay max

puts "--- report_checks -format json -sort_by_slack ---"
report_checks -format json -sort_by_slack

puts "--- report_checks min with fields ---"
report_checks -path_delay min -fields {capacitance slew fanout input_pin net}

puts "--- report_checks with -digits ---"
report_checks -format json -digits 6
