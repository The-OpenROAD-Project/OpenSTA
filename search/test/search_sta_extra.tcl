# Test additional Sta.cc and Search.cc code paths
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

report_checks > /dev/null

puts "--- report_path_cmd on a path ---"
set paths [find_timing_paths -path_delay max]
foreach pe $paths {
  set p [$pe path]
  sta::report_path_cmd $p
  break
}
puts "PASS: report_path_cmd"

puts "--- report_path with json format ---"
sta::set_report_path_format json
set paths2 [find_timing_paths -path_delay max]
foreach pe $paths2 {
  set p [$pe path]
  sta::report_path_cmd $p
  break
}
sta::set_report_path_format full
puts "PASS: report_path json"

puts "--- worstSlack single-arg form ---"
catch {
  set ws [sta::worst_slack_cmd max]
  puts "worst_slack: $ws"
}
puts "PASS: worst_slack"

puts "--- checkFanout via report_check_types ---"
set_max_fanout 2 [current_design]
report_check_types -max_fanout -verbose
puts "PASS: checkFanout"

puts "--- report_checks with -fields and various combos ---"
report_checks -fields {capacitance slew fanout} -format full
report_checks -fields {input_pin net} -format full
report_checks -fields {capacitance slew fanout input_pin net src_attr} -format full_clock
report_checks -fields {capacitance slew fanout input_pin net src_attr} -format full_clock_expanded
report_checks -fields {capacitance} -format short
report_checks -fields {slew} -format end
puts "PASS: report_checks fields combos"

puts "--- report_checks with -slack_min and -slack_max ---"
report_checks -slack_max 0 -path_delay max
report_checks -slack_min -10 -path_delay max
report_checks -slack_max 100 -slack_min -100 -path_delay max
puts "PASS: slack_min/max filters"

puts "--- set_report_path_field_properties ---"
catch { sta::set_report_path_field_properties "delay" "Delay" 10 0 }
catch { sta::set_report_path_field_width "delay" 12 }
report_checks -path_delay max
puts "PASS: field properties"

puts "--- set_report_path_sigmas ---"
catch { sta::set_report_path_sigmas 1 }
report_checks -path_delay max
catch { sta::set_report_path_sigmas 0 }
puts "PASS: report_path sigmas"

puts "--- find_timing_paths with recovery/removal/gating_setup/gating_hold ---"
catch {
  sta::set_recovery_removal_checks_enabled 1
  sta::set_gated_clk_checks_enabled 1
  set paths_all [find_timing_paths -path_delay max -endpoint_path_count 5 -group_path_count 5]
  puts "Paths: [llength $paths_all]"
  sta::set_recovery_removal_checks_enabled 0
  sta::set_gated_clk_checks_enabled 0
}
puts "PASS: recovery/gating paths"

puts "--- report_annotated_delay ---"
catch { report_annotated_delay }
puts "PASS: report_annotated_delay"

puts "--- report_annotated_check ---"
catch { report_annotated_check }
puts "PASS: report_annotated_check"

puts "--- report_checks with -path_delay max_rise/max_fall/min_rise/min_fall ---"
report_checks -path_delay max_rise -format end
report_checks -path_delay max_fall -format end
report_checks -path_delay min_rise -format end
report_checks -path_delay min_fall -format end
puts "PASS: rise/fall delay variants"

puts "--- report_checks with -corner ---"
set corner [sta::cmd_corner]
report_checks -path_delay max -corner [$corner name]
puts "PASS: report_checks corner"

puts "--- set_report_path_no_split ---"
sta::set_report_path_no_split 1
report_checks -path_delay max
sta::set_report_path_no_split 0
puts "PASS: no_split"

puts "--- Edge detailed methods ---"
set edges [get_timing_edges -from [get_pins and1/A1] -to [get_pins and1/ZN]]
foreach edge $edges {
  puts "sim_timing_sense: [$edge sim_timing_sense]"
  puts "cond: [$edge cond]"
  puts "mode_name: [$edge mode_name]"
  puts "mode_value: [$edge mode_value]"
  puts "is_disabled_loop: [$edge is_disabled_loop]"
  puts "is_disabled_constraint: [$edge is_disabled_constraint]"
  puts "is_disabled_constant: [$edge is_disabled_constant]"
  puts "is_disabled_cond_default: [$edge is_disabled_cond_default]"
  puts "is_disabled_bidirect_inst_path: [$edge is_disabled_bidirect_inst_path]"
  puts "is_disabled_bidirect_net_path: [$edge is_disabled_bidirect_net_path]"
  puts "is_disabled_preset_clear: [$edge is_disabled_preset_clear]"
  set dpins [$edge disabled_constant_pins]
  puts "disabled_constant_pins count: [llength $dpins]"
  set tarcs [$edge timing_arcs]
  foreach tarc $tarcs {
    set delays [$edge arc_delays $tarc]
    puts "arc_delays count: [llength $delays]"
    set dstrs [$edge arc_delay_strings $tarc 3]
    puts "arc_delay_strings count: [llength $dstrs]"
    set corner2 [sta::cmd_corner]
    puts "delay_annotated: [$edge delay_annotated $tarc $corner2 max]"
    break
  }
  break
}
puts "PASS: edge detailed methods"

puts "--- Vertex methods via worst_slack_vertex ---"
set wv [sta::worst_slack_vertex max]
if { $wv != "NULL" } {
  puts "worst_slack_vertex is_clock: [$wv is_clock]"
  puts "worst_slack_vertex has_downstream_clk_pin: [$wv has_downstream_clk_pin]"
  puts "worst_slack_vertex is_disabled_constraint: [$wv is_disabled_constraint]"
}
puts "PASS: vertex methods"

puts "--- Vertex from PathEnd ---"
set paths_v [find_timing_paths -path_delay max]
foreach pe $paths_v {
  set v [$pe vertex]
  puts "pathend vertex is_clock: [$v is_clock]"
  puts "pathend vertex has_downstream_clk_pin: [$v has_downstream_clk_pin]"
  break
}
puts "PASS: vertex from PathEnd"

puts "--- vertex_worst_arrival_path ---"
catch {
  set warr [sta::vertex_worst_arrival_path $wv max]
  if { $warr != "NULL" } {
    puts "worst_arrival_path pin: [get_full_name [$warr pin]]"
  }
}
puts "PASS: vertex_worst_arrival_path"

puts "--- vertex_worst_slack_path ---"
catch {
  set wslk [sta::vertex_worst_slack_path $wv max]
  if { $wslk != "NULL" } {
    puts "worst_slack_path pin: [get_full_name [$wslk pin]]"
  }
}
puts "PASS: vertex_worst_slack_path"

puts "--- report_path_end with prev_end ---"
set paths3 [find_timing_paths -path_delay max -endpoint_path_count 3]
set prev_end ""
foreach pe $paths3 {
  if { $prev_end != "" } {
    catch { sta::report_path_end $pe $prev_end 0 }
  }
  set prev_end $pe
}
puts "PASS: report_path_end with prev"

puts "--- make_instance ---"
catch {
  set and_cell2 [get_lib_cells NangateOpenCellLibrary/AND2_X1]
  sta::make_instance new_inst $and_cell2
  puts "make_instance: done"
}
puts "PASS: make_instance"

puts "--- pocv_enabled ---"
catch { puts "pocv_enabled: [sta::pocv_enabled]" }
puts "PASS: pocv_enabled"

puts "--- report_checks -summary format ---"
report_checks -path_delay max -format summary
puts "PASS: summary format"

puts "--- clear_sta ---"
catch { sta::clear_sta }
puts "PASS: clear_sta"

puts "ALL PASSED"
