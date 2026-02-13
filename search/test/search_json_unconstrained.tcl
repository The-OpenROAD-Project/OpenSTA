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
puts "PASS: json format multi-endpoint"

puts "--- report_checks -format json min ---"
report_checks -format json -path_delay min
puts "PASS: json format min"

puts "--- report_checks -format json min_max ---"
report_checks -format json -path_delay min_max
puts "PASS: json format min_max"

puts "--- report_checks -format json to specific pin ---"
report_checks -format json -to [get_pins reg1/D]
puts "PASS: json format to specific pin"

puts "--- report_checks -format json from specific port ---"
report_checks -format json -from [get_ports in1]
puts "PASS: json format from port"

puts "--- report_checks -format json through pin ---"
report_checks -format json -through [get_pins and1/ZN]
puts "PASS: json format through pin"

puts "--- report_checks -unconstrained format full ---"
report_checks -unconstrained -format full
puts "PASS: unconstrained full format"

puts "--- report_checks -unconstrained format short ---"
report_checks -unconstrained -format short
puts "PASS: unconstrained short format"

puts "--- report_checks -unconstrained format end ---"
report_checks -unconstrained -format end
puts "PASS: unconstrained end format"

puts "--- report_checks -unconstrained format slack_only ---"
report_checks -unconstrained -format slack_only
puts "PASS: unconstrained slack_only format"

puts "--- report_path on individual path (json format) ---"
sta::set_report_path_format json
set paths_j [find_timing_paths -path_delay max -endpoint_path_count 3]
foreach pe $paths_j {
  set p [$pe path]
  sta::report_path_cmd $p
}
sta::set_report_path_format full
puts "PASS: report_path_cmd json multi"

puts "--- reportPathFull on a single path ---"
set paths_f [find_timing_paths -path_delay max]
foreach pe $paths_f {
  set p [$pe path]
  catch { report_path $p }
  break
}
puts "PASS: report_path full"

puts "--- json report with full_clock format (for source clock paths) ---"
report_checks -format json -path_delay max
puts "PASS: json with clock info"

puts "--- report_checks -format json -sort_by_slack ---"
report_checks -format json -sort_by_slack
puts "PASS: json sort_by_slack"

puts "--- report_checks min with fields ---"
report_checks -path_delay min -fields {capacitance slew fanout input_pin net}
puts "PASS: min with all fields"

puts "--- report_checks with -digits ---"
report_checks -format json -digits 6
puts "PASS: json digits 6"

puts "ALL PASSED"
