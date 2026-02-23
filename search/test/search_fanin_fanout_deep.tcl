# Test deeper fanin/fanout search, Tag.cc, Path.cc, vertex slew queries,
# and various Sta.cc query functions.
# Targets: Sta.cc findFaninPins, findFaninInstances, findFanoutPins,
#          findFanoutInstances, vertexSlew, vertexArrival, vertexRequired,
#          vertexSlack, netSlack, pinSlack, endpointSlack, findRequired,
#          Tag.cc tag operations, Path.cc path access
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_limit_violations.v
link_design search_limit_violations

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_input_delay -clock clk 1.0 [get_ports in3]
set_input_delay -clock clk 1.0 [get_ports in4]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]
set_output_delay -clock clk 2.0 [get_ports out3]

# Run timing
report_checks -path_delay max > /dev/null

puts "=== FANIN/FANOUT SEARCH ==="

puts "--- get_fanin of register ---"
set fanin_pins [get_fanin -to [get_pins reg1/D]]
puts "Fanin pins of reg1/D: [llength $fanin_pins]"
foreach p $fanin_pins { puts "  [get_full_name $p]" }

puts "--- get_fanin -flat ---"
set fanin_flat [get_fanin -to [get_pins reg1/D] -flat]
puts "Fanin flat of reg1/D: [llength $fanin_flat]"
foreach p $fanin_flat { puts "  [get_full_name $p]" }

puts "--- get_fanin -startpoints_only ---"
set fanin_start [get_fanin -to [get_pins reg1/D] -startpoints_only]
puts "Fanin startpoints of reg1/D: [llength $fanin_start]"
foreach p $fanin_start { puts "  [get_full_name $p]" }

puts "--- get_fanin -only_cells ---"
set fanin_cells [get_fanin -to [get_pins reg1/D] -only_cells]
puts "Fanin cells of reg1/D: [llength $fanin_cells]"
foreach c $fanin_cells { puts "  [get_full_name $c]" }

puts "--- get_fanin with -pin_levels ---"
set fanin_lev [get_fanin -to [get_pins reg1/D] -pin_levels 2]
puts "Fanin 2 levels of reg1/D: [llength $fanin_lev]"
foreach p $fanin_lev { puts "  [get_full_name $p]" }

puts "--- get_fanin with -levels ---"
set fanin_inst_lev [get_fanin -to [get_pins reg1/D] -levels 2]
puts "Fanin 2 inst levels of reg1/D: [llength $fanin_inst_lev]"
foreach p $fanin_inst_lev { puts "  [get_full_name $p]" }

puts "--- get_fanout of driver pin ---"
set fanout_pins [get_fanout -from [get_pins inv2/ZN]]
puts "Fanout pins of inv2/ZN: [llength $fanout_pins]"
foreach p $fanout_pins { puts "  [get_full_name $p]" }

puts "--- get_fanout -flat ---"
set fanout_flat [get_fanout -from [get_pins and2/ZN] -flat]
puts "Fanout flat of and2/ZN: [llength $fanout_flat]"
foreach p $fanout_flat { puts "  [get_full_name $p]" }

puts "--- get_fanout -endpoints_only ---"
set fanout_end [get_fanout -from [get_pins and2/ZN] -endpoints_only]
puts "Fanout endpoints of and2/ZN: [llength $fanout_end]"
foreach p $fanout_end { puts "  [get_full_name $p]" }

puts "--- get_fanout -only_cells ---"
set fanout_cells [get_fanout -from [get_pins inv2/ZN] -only_cells]
puts "Fanout cells of inv2/ZN: [llength $fanout_cells]"
foreach c $fanout_cells { puts "  [get_full_name $c]" }

puts "--- get_fanout with -pin_levels ---"
set fanout_lev [get_fanout -from [get_pins and2/ZN] -pin_levels 2]
puts "Fanout 2 levels of and2/ZN: [llength $fanout_lev]"
foreach p $fanout_lev { puts "  [get_full_name $p]" }

puts "--- get_fanout with -levels ---"
set fanout_inst_lev [get_fanout -from [get_pins and2/ZN] -levels 2]
puts "Fanout 2 inst levels of and2/ZN: [llength $fanout_inst_lev]"
foreach p $fanout_inst_lev { puts "  [get_full_name $p]" }

puts "=== VERTEX/PIN QUERIES ==="

puts "--- Pin arrival ---"
report_arrival [get_pins reg1/D]
report_arrival [get_pins reg1/Q]
report_arrival [get_pins and1/ZN]
report_arrival [get_ports in1]
report_arrival [get_ports out1]

puts "--- Pin required ---"
report_required [get_pins reg1/D]
report_required [get_ports out1]

puts "--- Pin slack ---"
report_slack [get_pins reg1/D]
report_slack [get_ports out1]

puts "--- Pin slack various ---"
set ps1 [get_property [get_pins and1/ZN] slack_max_rise]
puts "and1/ZN max rise slack: $ps1"
set ps2 [get_property [get_pins inv2/ZN] slack_min_fall]
puts "inv2/ZN min fall slack: $ps2"

puts "=== PATH QUERY ==="

puts "--- find_timing_paths and path details ---"
set paths [find_timing_paths -path_delay max -endpoint_count 5 -group_path_count 10]
puts "Found [llength $paths] paths"
foreach pe $paths {
  set p [$pe path]
  puts "  endpoint: [get_full_name [$pe pin]]"
  puts "    slack: [$pe slack]"
  puts "    arrival: [$p arrival]"
  puts "    required: [$p required]"
  set pins [$p pins]
  puts "    path_pins: [llength $pins]"
  set sp [$p start_path]
  if { $sp != "NULL" } {
    puts "    start_pin: [get_full_name [$sp pin]]"
  }
}

puts "--- worst_slack_vertex ---"
set wv [sta::worst_slack_vertex max]
if { $wv != "NULL" } {
  puts "Worst slack vertex: [get_full_name [$wv pin]]"
  set warr [sta::vertex_worst_arrival_path $wv max]
  if { $warr != "NULL" } {
    puts "  arrival: [$warr arrival]"
  }
  set wslk [sta::vertex_worst_slack_path $wv max]
  if { $wslk != "NULL" } {
    puts "  slack: [$wslk slack]"
  }
}

puts "--- find_requireds ---"
sta::find_requireds

puts "=== REPORT DEBUG ==="

puts "--- tag/clk_info counts ---"
puts "tag_group_count: [sta::tag_group_count]"
puts "tag_count: [sta::tag_count]"
puts "clk_info_count: [sta::clk_info_count]"
puts "path_count: [sta::path_count]"
puts "endpoint_violation_count max: [sta::endpoint_violation_count max]"
puts "endpoint_violation_count min: [sta::endpoint_violation_count min]"

puts "--- report_path_cmd ---"
set paths2 [find_timing_paths -path_delay max]
foreach pe $paths2 {
  set p [$pe path]
  sta::report_path_cmd $p
  break
}

puts "--- get_fanin with -trace_arcs all (thru disabled/constants) ---"
set fanin_thru [get_fanin -to [get_pins reg1/D] -trace_arcs all]
puts "Fanin trace_arcs all: [llength $fanin_thru]"

puts "--- get_fanin with -trace_arcs timing ---"
set fanin_timing [get_fanin -to [get_pins reg1/D] -trace_arcs timing]
puts "Fanin trace_arcs timing: [llength $fanin_timing]"

puts "--- get_fanin with -trace_arcs enabled ---"
set fanin_enabled [get_fanin -to [get_pins reg1/D] -trace_arcs enabled]
puts "Fanin trace_arcs enabled: [llength $fanin_enabled]"

puts "--- get_fanin thru constants ---"
set_case_analysis 1 [get_ports in1]
set fanin_const [get_fanin -to [get_pins reg1/D] -trace_arcs all]
puts "Fanin with constants: [llength $fanin_const]"
unset_case_analysis [get_ports in1]
