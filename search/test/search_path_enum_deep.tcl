# Test PathEnum.cc deeper: path enumeration with endpoint_count > 1,
# diversion queue, unique_pins pruning, and path comparison.
# Also exercises Search.cc visitPathEnds dispatching and PathGroup
# path end sorting/pruning with multiple groups.
# Targets: PathEnum.cc insert, findNext, makeDiversions,
#   pruneDiversionQueue, hasNext, next, DiversionGreater,
#   PathEnumFaninVisitor,
#   Search.cc findPathEnds, visitPathEnds (multiple path groups),
#   PathGroup.cc sort, prune, enumPathEnds
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

############################################################
# Path enumeration with endpoint_count > 1
# (exercises PathEnum deeply: diversions, findNext, etc.)
############################################################
puts "--- find_timing_paths endpoint_count 5 ---"
set paths [find_timing_paths -path_delay max -endpoint_count 5 -group_path_count 20]
puts "epc 5 paths: [llength $paths]"
foreach pe $paths {
  puts "  [get_full_name [$pe pin]] slack=[$pe slack]"
}

puts "--- find_timing_paths endpoint_count 3 group_path_count 10 ---"
set paths2 [find_timing_paths -path_delay max -endpoint_count 3 -group_path_count 10]
puts "epc 3 gpc 10: [llength $paths2]"

puts "--- find_timing_paths min endpoint_count 5 ---"
set paths_min [find_timing_paths -path_delay min -endpoint_count 5 -group_path_count 20]
puts "min epc 5: [llength $paths_min]"
foreach pe $paths_min {
  puts "  [get_full_name [$pe pin]] slack=[$pe slack]"
}

############################################################
# Path enumeration with endpoint_count 1 (default)
############################################################
puts "--- find_timing_paths endpoint_count 1 ---"
set paths3 [find_timing_paths -path_delay max -endpoint_count 1 -group_path_count 20]
puts "epc 1: [llength $paths3]"

############################################################
# Unique paths to endpoint with multiple paths
############################################################
puts "--- -unique_paths_to_endpoint epc 3 ---"
set paths_u [find_timing_paths -path_delay max -endpoint_count 3 -group_path_count 15 -unique_paths_to_endpoint]
puts "unique epc 3: [llength $paths_u]"

puts "--- -unique_edges_to_endpoint epc 3 ---"
set paths_ue [find_timing_paths -path_delay max -endpoint_count 3 -group_path_count 15 -unique_edges_to_endpoint]
puts "unique_edges epc 3: [llength $paths_ue]"

############################################################
# Sort by slack with multiple paths
############################################################
puts "--- -sort_by_slack endpoint_count 5 ---"
set paths_s [find_timing_paths -path_delay max -endpoint_count 5 -group_path_count 20 -sort_by_slack]
puts "sorted epc 5: [llength $paths_s]"
set prev_slack 999999
set ok 1
foreach pe $paths_s {
  set s [$pe slack]
  if { $s > $prev_slack } { set ok 0 }
  set prev_slack $s
}
puts "Sorted correctly: $ok"

############################################################
# Group paths + enumeration
############################################################
group_path -name in_grp -from [get_ports {in1 in2}]
group_path -name out_grp -to [get_ports {out1 out2 out3}]

puts "--- find_timing_paths grouped epc 5 ---"
set paths_g [find_timing_paths -path_delay max -endpoint_count 5 -group_path_count 15]
puts "grouped epc 5: [llength $paths_g]"

############################################################
# report_checks with endpoint_count (text output)
############################################################
puts "--- report_checks epc 3 -fields ---"
report_checks -path_delay max -endpoint_count 3 -fields {slew cap input_pins nets fanout}

puts "--- report_checks epc 3 -format end ---"
report_checks -path_delay max -endpoint_count 3 -format end

puts "--- report_checks epc 3 -format summary ---"
report_checks -path_delay max -endpoint_count 3 -format summary

puts "--- report_checks min epc 3 ---"
report_checks -path_delay min -endpoint_count 3 -fields {slew cap}

############################################################
# report_checks with -from/-to + endpoint_count
############################################################
puts "--- report_checks epc 3 -from -to ---"
report_checks -path_delay max -endpoint_count 3 -from [get_ports in1] -to [get_pins reg1/D]

puts "--- report_checks epc 3 -through ---"
report_checks -path_delay max -endpoint_count 3 -through [get_pins and1/ZN]

############################################################
# Path delay variants: max_rise, max_fall, min_rise, min_fall
############################################################
puts "--- find_timing_paths -path_delay max_rise ---"
set pr [find_timing_paths -path_delay max_rise -endpoint_count 3]
puts "max_rise paths: [llength $pr]"

puts "--- find_timing_paths -path_delay max_fall ---"
set pf [find_timing_paths -path_delay max_fall -endpoint_count 3]
puts "max_fall paths: [llength $pf]"

puts "--- find_timing_paths -path_delay min_rise ---"
set pmr [find_timing_paths -path_delay min_rise -endpoint_count 3]
puts "min_rise paths: [llength $pmr]"

puts "--- find_timing_paths -path_delay min_fall ---"
set pmf [find_timing_paths -path_delay min_fall -endpoint_count 3]
puts "min_fall paths: [llength $pmf]"

puts "--- find_timing_paths -path_delay min_max ---"
set pmm [find_timing_paths -path_delay min_max -endpoint_count 3]
puts "min_max paths: [llength $pmm]"

############################################################
# Large endpoint_count to exercise limits
############################################################
puts "--- find_timing_paths epc 10 gpc 50 ---"
set paths_big [find_timing_paths -path_delay max -endpoint_count 10 -group_path_count 50]
puts "big epc: [llength $paths_big]"

############################################################
# Slack filtering
############################################################
puts "--- slack_max filtering ---"
set ps1 [find_timing_paths -path_delay max -slack_max 0.0]
puts "slack <= 0: [llength $ps1]"
set ps2 [find_timing_paths -path_delay max -slack_max 100.0 -endpoint_count 5]
puts "slack <= 100: [llength $ps2]"

puts "--- slack_min filtering ---"
set ps3 [find_timing_paths -path_delay max -slack_min -100.0 -endpoint_count 5]
puts "slack >= -100: [llength $ps3]"
