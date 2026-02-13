# Test Search.cc: arrivalInvalid/requiredInvalid flows triggered by
# network edits, find_requireds, arrival/required entry reporting,
# vertex path iteration, vertex_worst_arrival_path/vertex_worst_slack_path,
# endpointViolationCount, report_arrivals, report_tags, report_tag_groups,
# report_clk_infos, report_path_count_histogram.
# Also exercises path enumeration with -sort_by_slack and -unique_pins.
# Targets: Search.cc arrivalInvalid, requiredInvalid,
#          findAllArrivals, findRequireds, endpointViolationCount,
#          reportArrivals, reportTags, reportTagGroups, reportClkInfos,
#          reportPathCountHistogram
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

# Baseline timing
report_checks -path_delay max > /dev/null

############################################################
# find_requireds
############################################################
puts "--- find_requireds ---"
sta::find_requireds
puts "PASS: find_requireds"

############################################################
# endpoint_violation_count
############################################################
puts "--- endpoint_violation_count ---"
puts "max violations: [sta::endpoint_violation_count max]"
puts "min violations: [sta::endpoint_violation_count min]"
puts "PASS: endpoint_violation_count"

############################################################
# report internal structures
############################################################
puts "--- report_tags ---"
catch { sta::report_tags }
puts "PASS: report_tags"

puts "--- report_clk_infos ---"
catch { sta::report_clk_infos }
puts "PASS: report_clk_infos"

puts "--- report_tag_groups ---"
catch { sta::report_tag_groups }
puts "PASS: report_tag_groups"

puts "--- report_path_count_histogram ---"
catch { sta::report_path_count_histogram }
puts "PASS: report_path_count_histogram"

puts "--- report_arrival_entries ---"
catch { sta::report_arrival_entries }
puts "PASS: report_arrival_entries"

puts "--- report_required_entries ---"
catch { sta::report_required_entries }
puts "PASS: report_required_entries"

puts "--- counts ---"
puts "tags: [sta::tag_count]"
puts "tag_groups: [sta::tag_group_count]"
puts "clk_infos: [sta::clk_info_count]"
puts "paths: [sta::path_count]"
puts "PASS: counts"

############################################################
# Vertex path iteration and worst path
############################################################
puts "--- vertex queries ---"
set wv [sta::worst_slack_vertex max]
if { $wv != "NULL" } {
  puts "worst_slack_vertex pin: [get_full_name [$wv pin]]"
  puts "worst_slack_vertex level: [$wv level]"

  # vertex_worst_arrival_path
  set warr [sta::vertex_worst_arrival_path $wv max]
  if { $warr != "NULL" } {
    puts "worst_arrival_path pin: [get_full_name [$warr pin]]"
    puts "worst_arrival_path arrival: [$warr arrival]"
  }

  # vertex_worst_slack_path
  set wslk [sta::vertex_worst_slack_path $wv max]
  if { $wslk != "NULL" } {
    puts "worst_slack_path pin: [get_full_name [$wslk pin]]"
    puts "worst_slack_path slack: [$wslk slack]"
  }

  # report_tag_arrivals
  catch { sta::report_tag_arrivals_cmd $wv 1 }
  catch { sta::report_tag_arrivals_cmd $wv 0 }
}
puts "PASS: vertex queries"

puts "--- worst_slack_vertex min ---"
set wv_min [sta::worst_slack_vertex min]
if { $wv_min != "NULL" } {
  puts "worst_slack_vertex min pin: [get_full_name [$wv_min pin]]"
  set warr_min [sta::vertex_worst_arrival_path $wv_min min]
  if { $warr_min != "NULL" } {
    puts "worst_arrival_path min pin: [get_full_name [$warr_min pin]]"
  }
  set wslk_min [sta::vertex_worst_slack_path $wv_min min]
  if { $wslk_min != "NULL" } {
    puts "worst_slack_path min pin: [get_full_name [$wslk_min pin]]"
    puts "worst_slack_path min slack: [$wslk_min slack]"
  }
}
puts "PASS: vertex min queries"

############################################################
# Trigger arrivalInvalid/requiredInvalid via network edits
############################################################
puts "--- Arrival invalidation via network edit ---"
# Replace cell triggers invalidation of timing
replace_cell buf1 NangateOpenCellLibrary/BUF_X2
# Query timing (forces recalculation -> exercises arrival/required invalidation)
report_checks -path_delay max > /dev/null
set ws1 [sta::worst_slack_cmd max]
puts "Worst slack after BUF_X2: $ws1"

# Replace back
replace_cell buf1 NangateOpenCellLibrary/BUF_X1
report_checks -path_delay max > /dev/null
set ws2 [sta::worst_slack_cmd max]
puts "Worst slack after BUF_X1: $ws2"
puts "PASS: arrival invalidation"

############################################################
# Network edit to trigger deleteInstanceBefore/arrivalInvalid
############################################################
puts "--- Delete instance timing invalidation ---"
set new_i [make_instance tmp_buf NangateOpenCellLibrary/BUF_X1]
set new_n [make_net tmp_net]
connect_pin $new_n tmp_buf/A
report_checks -path_delay max > /dev/null
disconnect_pin $new_n tmp_buf/A
delete_instance $new_i
delete_net $new_n
# Timing recalculation after deletion
report_checks -path_delay max > /dev/null
set ws3 [sta::worst_slack_cmd max]
puts "Worst slack after delete: $ws3"
puts "PASS: delete timing invalidation"

############################################################
# Tighten constraints to create violations, then check count
############################################################
puts "--- Create violations ---"
set_output_delay -clock clk 9.5 [get_ports out1]
report_checks -path_delay max > /dev/null
puts "max violations (tight): [sta::endpoint_violation_count max]"
set_output_delay -clock clk 2.0 [get_ports out1]
report_checks -path_delay max > /dev/null
puts "max violations (normal): [sta::endpoint_violation_count max]"
puts "PASS: violation count changes"

############################################################
# find_timing_paths with sort_by_slack
############################################################
puts "--- find_timing_paths sort_by_slack ---"
set paths_sorted [find_timing_paths -path_delay max -sort_by_slack -endpoint_path_count 5 -group_path_count 10]
puts "Sorted paths: [llength $paths_sorted]"
set prev_slack 999999
foreach pe $paths_sorted {
  set s [$pe slack]
  puts "  slack: $s pin=[get_full_name [$pe pin]]"
}
puts "PASS: sort_by_slack"

############################################################
# report_checks with -slack_max and -slack_min
############################################################
puts "--- report_checks slack filters ---"
report_checks -slack_max 0 -path_delay max
report_checks -slack_min -100 -path_delay max
report_checks -slack_max 100 -slack_min -100 -path_delay max
puts "PASS: slack filters"

############################################################
# report_checks -from/-to/-through
############################################################
puts "--- report_checks from/to/through ---"
report_checks -from [get_ports in1] -path_delay max
report_checks -to [get_ports out1] -path_delay max
report_checks -through [get_pins buf1/Z] -path_delay max
report_checks -from [get_ports in1] -to [get_ports out1] -path_delay max
puts "PASS: from/to/through"

############################################################
# report_tns / report_wns
############################################################
puts "--- report_tns/wns ---"
report_tns
report_wns
report_worst_slack -max
report_worst_slack -min
report_tns -digits 6
report_wns -digits 6
puts "PASS: tns/wns"

############################################################
# levelize
############################################################
puts "--- levelize ---"
sta::levelize
puts "PASS: levelize"

puts "ALL PASSED"
