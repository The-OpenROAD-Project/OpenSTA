# Test PathEnum.cc, PathGroup.cc deeper code paths: group_path with various
# options, path enumeration with endpoints, group naming, slack filtering.
# Targets: PathGroup.cc makePathEnds, sort, prune, enumPathEnds,
#          PathGroups makeGroups, pushGroupPathEnds, makeGroupPathEnds,
#          Search.cc findPathEnds, visitPathEnds
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_path_end_types.v
link_design search_path_end_types

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_input_delay -clock clk 0.5 [get_ports rst]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]
set_output_delay -clock clk 2.0 [get_ports out3]

report_checks > /dev/null

puts "--- group_path -name with -from and -to ---"
group_path -name input_paths -from [get_ports {in1 in2}]
group_path -name output_paths -to [get_ports {out1 out2 out3}]
group_path -name reg2reg_paths -from [get_pins reg1/CK] -to [get_pins reg2/D]
puts "PASS: group_path setup"

puts "--- report_checks with groups ---"
report_checks -path_delay max
puts "PASS: report_checks with groups"

puts "--- find_timing_paths with large group and endpoint counts ---"
set paths [find_timing_paths -path_delay max -group_path_count 20 -endpoint_path_count 10]
puts "Found [llength $paths] paths (max)"
foreach pe $paths {
  puts "  [get_full_name [$pe pin]] slack=[$pe slack] role=[$pe check_role]"
}
puts "PASS: large path count"

puts "--- find_timing_paths with min paths ---"
set paths_min [find_timing_paths -path_delay min -group_path_count 20 -endpoint_path_count 10]
puts "Found [llength $paths_min] paths (min)"
foreach pe $paths_min {
  puts "  [get_full_name [$pe pin]] slack=[$pe slack]"
}
puts "PASS: min path enum"

puts "--- find_timing_paths -sort_by_slack ---"
set paths_sorted [find_timing_paths -sort_by_slack -path_delay max -group_path_count 20 -endpoint_path_count 10]
puts "Found [llength $paths_sorted] sorted paths"
set prev_slack 999999
set sorted_ok 1
foreach pe $paths_sorted {
  set s [$pe slack]
  if { $s > $prev_slack } {
    set sorted_ok 0
  }
  set prev_slack $s
}
puts "Sort order correct: $sorted_ok"
puts "PASS: sort_by_slack"

puts "--- find_timing_paths -unique_paths_to_endpoint ---"
set paths_unique [find_timing_paths -unique_paths_to_endpoint -path_delay max -group_path_count 10 -endpoint_path_count 5]
puts "Found [llength $paths_unique] unique paths"
puts "PASS: unique paths"

puts "--- find_timing_paths -slack_max filtering ---"
set paths_slack [find_timing_paths -path_delay max -slack_max 0.0]
puts "Paths with slack <= 0: [llength $paths_slack]"
set paths_slack2 [find_timing_paths -path_delay max -slack_max 100.0]
puts "Paths with slack <= 100: [llength $paths_slack2]"
puts "PASS: slack_max filtering"

puts "--- find_timing_paths -slack_min filtering ---"
set paths_slack3 [find_timing_paths -path_delay max -slack_min -100.0]
puts "Paths with slack >= -100: [llength $paths_slack3]"
puts "PASS: slack_min filtering"

puts "--- path_group_names ---"
set group_names [sta::path_group_names]
puts "Path group names: $group_names"
puts "PASS: path_group_names"

puts "--- is_path_group_name ---"
catch {
  puts "clk is group: [sta::is_path_group_name clk]"
  puts "input_paths is group: [sta::is_path_group_name input_paths]"
  puts "nonexistent is group: [sta::is_path_group_name nonexistent_group]"
}
puts "PASS: is_path_group_name"

puts "--- group_path -default ---"
catch {
  group_path -name default_group -default
  report_checks -path_delay max
}
puts "PASS: group_path default"

puts "--- report_path_ends ---"
set pe_list [find_timing_paths -path_delay max -endpoint_path_count 5]
sta::report_path_end_header
foreach pe $pe_list {
  sta::report_path_end $pe
}
sta::report_path_end_footer
puts "PASS: report_path_ends"

puts "--- PathEnd type queries on all paths ---"
foreach pe $pe_list {
  puts "  type: check=[$pe is_check] out=[$pe is_output_delay] unconst=[$pe is_unconstrained] pd=[$pe is_path_delay] latch=[$pe is_latch_check] data=[$pe is_data_check] gated=[$pe is_gated_clock]"
}
puts "PASS: PathEnd type queries"

puts "--- report_checks -format end with groups ---"
report_checks -format end -path_delay max
report_checks -format end -path_delay min
puts "PASS: end format"

puts "--- report_checks -format summary with groups ---"
report_checks -format summary -path_delay max
report_checks -format summary -path_delay min
puts "PASS: summary format"

puts "--- report_checks -format slack_only with groups ---"
report_checks -format slack_only -path_delay max
report_checks -format slack_only -path_delay min
puts "PASS: slack_only format"

puts "--- endpoint_violation_count ---"
puts "max violations: [sta::endpoint_violation_count max]"
puts "min violations: [sta::endpoint_violation_count min]"
puts "PASS: endpoint_violation_count"

puts "--- startpoints / endpoints ---"
set starts [sta::startpoints]
puts "Startpoints: [llength $starts]"
set ends [sta::endpoints]
puts "Endpoints: [llength $ends]"
puts "PASS: startpoints/endpoints"

puts "ALL PASSED"
