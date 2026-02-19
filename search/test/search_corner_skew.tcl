# Test Corner.cc, ClkSkew.cc, WorstSlack.cc, Sim.cc paths
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_crpr.v
link_design search_crpr

create_clock -name clk -period 10 [get_ports clk]
set_propagated_clock [get_clocks clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

# Run timing
report_checks > /dev/null

puts "--- Corner commands ---"
set corner [sta::cmd_corner]
puts "Corner name: [$corner name]"
puts "Multi corner: [sta::multi_corner]"

puts "--- ClkSkew report with propagated clock ---"
report_clock_skew -setup
report_clock_skew -hold
report_clock_skew -setup -include_internal_latency
report_clock_skew -hold -include_internal_latency

puts "--- ClkSkew with digits ---"
report_clock_skew -setup -digits 6
report_clock_skew -hold -digits 6

puts "--- report_clock_latency ---"
report_clock_latency
report_clock_latency -include_internal_latency
report_clock_latency -digits 6

puts "--- worst_slack corner-specific ---"
set corner [sta::cmd_corner]
set ws_corner [sta::worst_slack_corner $corner max]
puts "Worst slack corner max: $ws_corner"
set ws_corner_min [sta::worst_slack_corner $corner min]
puts "Worst slack corner min: $ws_corner_min"

puts "--- total_negative_slack corner ---"
set tns_corner [sta::total_negative_slack_corner_cmd $corner max]
puts "TNS corner max: $tns_corner"
set tns_corner_min [sta::total_negative_slack_corner_cmd $corner min]
puts "TNS corner min: $tns_corner_min"

puts "--- worst_slack_vertex ---"
set wv [sta::worst_slack_vertex max]
if { $wv != "NULL" } {
  puts "Worst vertex pin: [get_full_name [$wv pin]]"
}

puts "--- vertex_worst_arrival_path ---"
set wv [sta::worst_slack_vertex max]
if { $wv != "NULL" } {
  set wpath [sta::vertex_worst_arrival_path $wv max]
  if { $wpath != "NULL" } {
    puts "Worst arrival path pin: [get_full_name [$wpath pin]]"
    puts "Worst arrival: [$wpath arrival]"
  }
}

puts "--- vertex_worst_slack_path ---"
set wv [sta::worst_slack_vertex max]
if { $wv != "NULL" } {
  set wspath [sta::vertex_worst_slack_path $wv max]
  if { $wspath != "NULL" } {
    puts "Worst slack path pin: [get_full_name [$wspath pin]]"
    puts "Worst slack: [$wspath slack]"
  }
}

puts "--- set_case_analysis and sim ---"
set_case_analysis 0 [get_ports in2]
report_checks -path_delay max
set sv [sta::pin_sim_logic_value [get_pins and1/A2]]
puts "Sim value and1/A2: $sv"
report_constant [get_ports in2]
unset_case_analysis [get_ports in2]

puts "--- set_case_analysis 1 ---"
set_case_analysis 1 [get_ports in1]
report_checks -path_delay max
set sv1 [sta::pin_sim_logic_value [get_pins and1/A1]]
puts "Sim value and1/A1: $sv1"
unset_case_analysis [get_ports in1]

puts "--- report with clock_uncertainty ---"
set_clock_uncertainty 0.3 -setup [get_clocks clk]
set_clock_uncertainty 0.1 -hold [get_clocks clk]
report_checks -path_delay max -format full_clock_expanded
report_checks -path_delay min -format full_clock_expanded
report_clock_skew -setup
report_clock_skew -hold
unset_clock_uncertainty -setup [get_clocks clk]
unset_clock_uncertainty -hold [get_clocks clk]

puts "--- report with set_inter_clock_uncertainty ---"
set_clock_uncertainty -from [get_clocks clk] -to [get_clocks clk] -setup 0.15
report_checks -path_delay max -format full_clock_expanded
unset_clock_uncertainty -from [get_clocks clk] -to [get_clocks clk] -setup

puts "--- find_clk_min_period ---"
set min_period [sta::find_clk_min_period [get_clocks clk] 0]
puts "Min period: $min_period"
set min_period_port [sta::find_clk_min_period [get_clocks clk] 1]
puts "Min period (with port paths): $min_period_port"
