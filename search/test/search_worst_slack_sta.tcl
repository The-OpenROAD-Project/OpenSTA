# Test Sta.cc and WorstSlack.cc uncovered code paths:
# - Sta::worstSlack(MinMax) single-arg form
# - WorstSlack queue-related operations
# - Sta::checkFanout on specific pin
# - report_tns, report_wns with corner variants
# - Sta::findDelays level form
# - various report_checks combos that exercise uncovered lines
# Covers: Sta.cc worstSlack, checkFanout, WorstSlack.cc
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

# Force timing
report_checks > /dev/null

puts "--- worst_slack max ---"
set ws_max [sta::worst_slack_cmd max]
puts "worst_slack max: $ws_max"

puts "--- worst_slack min ---"
set ws_min [sta::worst_slack_cmd min]
puts "worst_slack min: $ws_min"

puts "--- total_negative_slack ---"
set tns_max [sta::total_negative_slack_cmd max]
puts "tns max: $tns_max"
set tns_min [sta::total_negative_slack_cmd min]
puts "tns min: $tns_min"

puts "--- total_negative_slack_corner ---"
set corner [sta::cmd_corner]
set tns_corner [sta::total_negative_slack_corner_cmd $corner max]
puts "tns corner max: $tns_corner"
set tns_corner_min [sta::total_negative_slack_corner_cmd $corner min]
puts "tns corner min: $tns_corner_min"

puts "--- worst_slack_corner ---"
set ws_corner_max [sta::worst_slack_corner $corner max]
puts "worst_slack corner max: $ws_corner_max"
set ws_corner_min [sta::worst_slack_corner $corner min]
puts "worst_slack corner min: $ws_corner_min"

puts "--- report_tns ---"
report_tns
report_tns -min
report_tns -max

puts "--- report_wns ---"
report_wns
report_wns -min
report_wns -max

puts "--- report_worst_slack ---"
report_worst_slack -min
report_worst_slack -max

puts "--- worst_slack_vertex ---"
set wv_max [sta::worst_slack_vertex max]
if { $wv_max != "NULL" } {
  puts "worst_slack_vertex max pin: [get_full_name [$wv_max pin]]"
  puts "  is_clock: [$wv_max is_clock]"
  puts "  has_downstream_clk_pin: [$wv_max has_downstream_clk_pin]"
}
set wv_min [sta::worst_slack_vertex min]
if { $wv_min != "NULL" } {
  puts "worst_slack_vertex min pin: [get_full_name [$wv_min pin]]"
}

puts "--- vertex_worst_arrival_path ---"
if { $wv_max != "NULL" } {
  catch {
    set warr [sta::vertex_worst_arrival_path $wv_max max]
    if { $warr != "NULL" } {
      puts "worst_arrival_path pin: [get_full_name [$warr pin]]"
      puts "worst_arrival_path arrival: [$warr arrival]"
    }
  }
}

puts "--- vertex_worst_slack_path ---"
if { $wv_max != "NULL" } {
  catch {
    set wslk [sta::vertex_worst_slack_path $wv_max max]
    if { $wslk != "NULL" } {
      puts "worst_slack_path pin: [get_full_name [$wslk pin]]"
      puts "worst_slack_path slack: [$wslk slack]"
    }
  }
}

puts "--- checkFanout (report_check_types -max_fanout) ---"
set_max_fanout 2 [current_design]
report_check_types -max_fanout
report_check_types -max_fanout -verbose
report_check_types -max_fanout -violators

puts "--- checkCapacitance ---"
set_max_capacitance 0.001 [current_design]
report_check_types -max_capacitance
report_check_types -max_capacitance -verbose

puts "--- checkSlew ---"
set_max_transition 0.1 [current_design]
report_check_types -max_slew
report_check_types -max_slew -verbose

puts "--- report_checks with various sorting ---"
report_checks -sort_by_slack -format end
report_checks -sort_by_slack -format slack_only
report_checks -sort_by_slack -format summary

puts "--- report_checks multi-path ---"
report_checks -path_delay max -endpoint_path_count 5 -group_path_count 5
report_checks -path_delay min -endpoint_path_count 5 -group_path_count 5

puts "--- report_checks unique_paths_to_endpoint ---"
report_checks -path_delay max -endpoint_path_count 3 -unique_paths_to_endpoint

puts "--- report_path_end with prev_end ---"
set paths [find_timing_paths -path_delay max -endpoint_path_count 5]
set prev_end ""
foreach pe $paths {
  if { $prev_end != "" } {
    catch { sta::report_path_end $pe $prev_end 0 }
  }
  set prev_end $pe
}

puts "--- path group names ---"
set groups [sta::path_group_names]
puts "path groups: $groups"

puts "--- report_checks with -corner ---"
set corner [sta::cmd_corner]
report_checks -path_delay max -corner [$corner name]
report_checks -path_delay min -corner [$corner name]

puts "--- design_power ---"
catch {
  set pwr [sta::design_power "NULL" "NULL"]
  puts "design_power: $pwr"
}

puts "--- set_report_path_field_properties ---"
catch { sta::set_report_path_field_properties "delay" "Dly" 10 0 }
report_checks -path_delay max > /dev/null
catch { sta::set_report_path_field_width "delay" 12 }
report_checks -path_delay max > /dev/null

puts "--- set_report_path_sigmas ---"
catch { sta::set_report_path_sigmas 1 }
report_checks -path_delay max > /dev/null
catch { sta::set_report_path_sigmas 0 }

puts "--- set_report_path_no_split ---"
sta::set_report_path_no_split 1
report_checks -path_delay max > /dev/null
sta::set_report_path_no_split 0

puts "--- graph loops ---"
catch {
  set loops [sta::graph_loop_count]
  puts "graph_loop_count: $loops"
}

puts "--- pocv ---"
catch { puts "pocv_enabled: [sta::pocv_enabled]" }

puts "--- report_annotated_delay ---"
catch { report_annotated_delay }

puts "--- report_annotated_check ---"
catch { report_annotated_check }
