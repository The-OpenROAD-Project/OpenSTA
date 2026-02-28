# Test PathEnum.cc deeply: endpoint_count with max_paths,
# DiversionGreater comparisons, unique_pins pruning, findNext iteration.
# This test specifically exercises PathEnum with large endpoint_count
# and group_path_count values to push diversion queue operations.
# Targets: PathEnum.cc PathEnum constructor, insert, findNext, makeDiversions,
#   pruneDiversionQueue, hasNext, next, DiversionGreater operator(),
#   PathEnumFaninVisitor, diversion path construction,
#   Search.cc findPathEnds, visitPathEnds with epc > 1,
#   PathGroup.cc compare, enumPathEnds, pushPathEnd, popPathEnd
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

# Run initial timing
report_checks > /dev/null

############################################################
# Large endpoint_count (key for PathEnum coverage)
# Exercises diversion queue more deeply
############################################################
puts "--- find_timing_paths epc 8 gpc 30 ---"
set paths_e8 [find_timing_paths -path_delay max -endpoint_count 8 -group_path_count 30]
puts "epc 8 gpc 30: [llength $paths_e8]"
foreach pe $paths_e8 {
  puts "  [get_full_name [$pe pin]] slack=[$pe slack]"
}

puts "--- find_timing_paths epc 15 gpc 50 ---"
set paths_e15 [find_timing_paths -path_delay max -endpoint_count 15 -group_path_count 50]
puts "epc 15 gpc 50: [llength $paths_e15]"

puts "--- find_timing_paths epc 20 gpc 100 ---"
set paths_e20 [find_timing_paths -path_delay max -endpoint_count 20 -group_path_count 100]
puts "epc 20 gpc 100: [llength $paths_e20]"

############################################################
# Min path delay with large epc
############################################################
puts "--- find_timing_paths min epc 8 gpc 30 ---"
set paths_min8 [find_timing_paths -path_delay min -endpoint_count 8 -group_path_count 30]
puts "min epc 8 gpc 30: [llength $paths_min8]"
foreach pe $paths_min8 {
  puts "  [get_full_name [$pe pin]] slack=[$pe slack]"
}

puts "--- find_timing_paths min epc 15 gpc 50 ---"
set paths_min15 [find_timing_paths -path_delay min -endpoint_count 15 -group_path_count 50]
puts "min epc 15 gpc 50: [llength $paths_min15]"

############################################################
# Unique paths with large epc (exercises pruning)
############################################################
puts "--- unique_paths_to_endpoint epc 10 ---"
set paths_u10 [find_timing_paths -path_delay max -endpoint_count 10 -group_path_count 30 -unique_paths_to_endpoint]
puts "unique epc 10: [llength $paths_u10]"

puts "--- unique_edges_to_endpoint epc 10 ---"
set paths_ue10 [find_timing_paths -path_delay max -endpoint_count 10 -group_path_count 30 -unique_edges_to_endpoint]
puts "unique_edges epc 10: [llength $paths_ue10]"

############################################################
# Sort by slack with large epc
############################################################
puts "--- sort_by_slack epc 10 gpc 30 ---"
set paths_s10 [find_timing_paths -path_delay max -endpoint_count 10 -group_path_count 30 -sort_by_slack]
puts "sorted epc 10: [llength $paths_s10]"
set prev_slack 999999
set ok 1
foreach pe $paths_s10 {
  set s [$pe slack]
  if { $s > $prev_slack } { set ok 0 }
  set prev_slack $s
}
puts "Sorted correctly: $ok"

############################################################
# report_checks with large epc
############################################################
puts "--- report_checks epc 8 -format full ---"
report_checks -path_delay max -endpoint_count 8 -format full

puts "--- report_checks epc 8 -format end ---"
report_checks -path_delay max -endpoint_count 8 -format end

puts "--- report_checks epc 8 -format summary ---"
report_checks -path_delay max -endpoint_count 8 -format summary

puts "--- report_checks min epc 8 ---"
report_checks -path_delay min -endpoint_count 8

############################################################
# Slack filtering with large epc
############################################################
puts "--- slack_max with epc 10 ---"
set ps_max [find_timing_paths -path_delay max -endpoint_count 10 -slack_max 100.0]
puts "slack<=100 epc 10: [llength $ps_max]"

puts "--- slack_min with epc 10 ---"
set ps_min [find_timing_paths -path_delay max -endpoint_count 10 -slack_min -100.0]
puts "slack>=-100 epc 10: [llength $ps_min]"

############################################################
# All path delay variants with large epc
############################################################
puts "--- max_rise epc 8 ---"
set pr [find_timing_paths -path_delay max_rise -endpoint_count 8]
puts "max_rise epc 8: [llength $pr]"

puts "--- max_fall epc 8 ---"
set pf [find_timing_paths -path_delay max_fall -endpoint_count 8]
puts "max_fall epc 8: [llength $pf]"

puts "--- min_rise epc 8 ---"
set pmr [find_timing_paths -path_delay min_rise -endpoint_count 8]
puts "min_rise epc 8: [llength $pmr]"

puts "--- min_fall epc 8 ---"
set pmf [find_timing_paths -path_delay min_fall -endpoint_count 8]
puts "min_fall epc 8: [llength $pmf]"

puts "--- min_max epc 8 ---"
set pmm [find_timing_paths -path_delay min_max -endpoint_count 8]
puts "min_max epc 8: [llength $pmm]"

############################################################
# Group paths with large epc
############################################################
puts "--- group_path + epc 10 ---"
group_path -name gp_in -from [get_ports {in1 in2}]
group_path -name gp_out -to [get_ports {out1 out2 out3}]
set paths_g [find_timing_paths -path_delay max -endpoint_count 10 -group_path_count 30]
puts "grouped epc 10: [llength $paths_g]"

############################################################
# -from/-to/-through with large epc
############################################################
puts "--- epc 8 -from -to ---"
report_checks -path_delay max -endpoint_count 8 -from [get_ports in1] -to [get_pins reg1/D]

puts "--- epc 8 -through ---"
report_checks -path_delay max -endpoint_count 8 -through [get_pins and1/ZN]

puts "--- epc 8 -rise_from ---"
set pr2 [find_timing_paths -path_delay max -endpoint_count 8 -rise_from [get_ports in1]]
puts "epc 8 rise_from: [llength $pr2]"

puts "--- epc 8 -fall_to ---"
set pf2 [find_timing_paths -path_delay max -endpoint_count 8 -fall_to [get_pins reg1/D]]
puts "epc 8 fall_to: [llength $pf2]"

puts "--- epc 8 -rise_through ---"
set prt [find_timing_paths -path_delay max -endpoint_count 8 -rise_through [get_pins and1/ZN]]
puts "epc 8 rise_through: [llength $prt]"

puts "--- epc 8 -fall_through ---"
set pft [find_timing_paths -path_delay max -endpoint_count 8 -fall_through [get_pins and1/ZN]]
puts "epc 8 fall_through: [llength $pft]"
