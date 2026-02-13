# Additional Property.cc tests - port slew/slack, liberty port, TimingArcSet
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

report_checks > /dev/null

puts "--- Port timing properties ---"
set port [get_ports in1]
puts "port direction: [get_property $port direction]"
puts "port port_direction: [get_property $port port_direction]"
puts "port slack_max: [get_property $port slack_max]"
puts "port slack_max_rise: [get_property $port slack_max_rise]"
puts "port slack_max_fall: [get_property $port slack_max_fall]"
puts "port slack_min: [get_property $port slack_min]"
puts "port slack_min_rise: [get_property $port slack_min_rise]"
puts "port slack_min_fall: [get_property $port slack_min_fall]"
puts "port slew_max: [get_property $port slew_max]"
puts "port slew_max_rise: [get_property $port slew_max_rise]"
puts "port slew_max_fall: [get_property $port slew_max_fall]"
puts "port slew_min: [get_property $port slew_min]"
puts "port slew_min_rise: [get_property $port slew_min_rise]"
puts "port slew_min_fall: [get_property $port slew_min_fall]"
puts "port activity: [get_property $port activity]"
set lp [get_property $port liberty_port]
if { $lp != "" && $lp != "NULL" } {
  puts "port liberty_port: [get_name $lp]"
} else {
  puts "port liberty_port: none"
}
puts "PASS: port timing properties"

puts "--- Output port properties ---"
set oport [get_ports out1]
puts "oport slack_max: [get_property $oport slack_max]"
puts "oport slack_min: [get_property $oport slack_min]"
puts "oport slew_max: [get_property $oport slew_max]"
puts "oport slew_min: [get_property $oport slew_min]"
puts "PASS: output port properties"

puts "--- LibertyPort extra properties ---"
set lport [get_lib_pins NangateOpenCellLibrary/AND2_X1/ZN]
puts "lport name: [get_property $lport name]"
puts "lport full_name: [get_property $lport full_name]"
puts "lport direction: [get_property $lport direction]"
catch { puts "lport function: [get_property $lport function]" }
catch { puts "lport capacitance: [get_property $lport capacitance]" }
catch { puts "lport max_capacitance: [get_property $lport max_capacitance]" }
catch { puts "lport max_transition: [get_property $lport max_transition]" }
catch { puts "lport is_register_clock: [get_property $lport is_register_clock]" }
catch { puts "lport is_clock: [get_property $lport is_clock]" }
puts "PASS: LibertyPort extra properties"

puts "--- LibertyCell extra properties ---"
set buf_cell [get_lib_cells NangateOpenCellLibrary/BUF_X1]
puts "buf is_buffer: [get_property $buf_cell is_buffer]"
set and_cell [get_lib_cells NangateOpenCellLibrary/AND2_X1]
puts "and is_buffer: [get_property $and_cell is_buffer]"
set dff_cell [get_lib_cells NangateOpenCellLibrary/DFF_X1]
puts "dff is_buffer: [get_property $dff_cell is_buffer]"
catch { puts "dff area: [get_property $dff_cell area]" }
catch { puts "dff cell_leakage_power: [get_property $dff_cell cell_leakage_power]" }
puts "PASS: LibertyCell extra properties"

puts "--- LibertyLibrary properties ---"
set lib [get_libs NangateOpenCellLibrary]
catch {
  set lib_cell_property [get_property -object_type lib $lib name]
  puts "lib by lib type: $lib_cell_property"
}
puts "PASS: LibertyLibrary properties"

puts "--- Clock extra properties ---"
set clk_obj [get_clocks clk]
puts "clock is_propagated: [get_property $clk_obj is_propagated]"
puts "PASS: Clock extra properties"

puts "--- TimingArcSet property via LibertyCell ---"
set and_cell2 [get_lib_cells NangateOpenCellLibrary/AND2_X1]
set arcsets [$and_cell2 timing_arc_sets]
foreach arcset $arcsets {
  set arcprop [sta::timing_arc_property $arcset full_name]
  puts "arc_set full_name: $arcprop"
  set arcprop2 [sta::timing_arc_property $arcset name]
  puts "arc_set name: $arcprop2"
}
puts "PASS: TimingArcSet property"

puts "--- Edge properties on different arc types ---"
# Setup check arcs
set ck_edges [get_timing_edges -from [get_pins reg1/CK]]
foreach edge $ck_edges {
  puts "edge: [get_property $edge full_name] sense=[get_property $edge sense]"
}
puts "PASS: CK edge properties"

puts "--- Edge properties on BUF arcs ---"
set buf_edges [get_timing_edges -from [get_pins buf1/A] -to [get_pins buf1/Z]]
foreach edge $buf_edges {
  puts "buf edge: [get_property $edge full_name]"
  puts "buf delay_max_rise: [get_property $edge delay_max_rise]"
  puts "buf delay_max_fall: [get_property $edge delay_max_fall]"
}
puts "PASS: BUF edge properties"

puts "--- Slew check limits ---"
set_max_transition 0.1 [current_design]
set slew_pins [sta::check_slew_limits "NULL" 1 "NULL" max]
puts "Slew limit violations: [llength $slew_pins]"
foreach p $slew_pins {
  sta::report_slew_limit_short_header
  sta::report_slew_limit_short $p "NULL" max
  sta::report_slew_limit_verbose $p "NULL" max
  break
}
puts "PASS: slew checks"

puts "--- Cap check limits ---"
set_max_capacitance 0.001 [current_design]
set cap_pins [sta::check_capacitance_limits "NULL" 1 "NULL" max]
puts "Cap limit violations: [llength $cap_pins]"
foreach p $cap_pins {
  sta::report_capacitance_limit_short_header
  sta::report_capacitance_limit_short $p "NULL" max
  sta::report_capacitance_limit_verbose $p "NULL" max
  break
}
puts "PASS: cap checks"

puts "--- Fanout check limits ---"
set_max_fanout 1 [current_design]
set fan_pins [sta::check_fanout_limits "NULL" 1 max]
puts "Fanout limit violations: [llength $fan_pins]"
foreach p $fan_pins {
  sta::report_fanout_limit_short_header
  sta::report_fanout_limit_short $p max
  sta::report_fanout_limit_verbose $p max
  break
}
puts "PASS: fanout checks"

puts "--- Slew/Cap/Fanout check slack ---"
catch {
  puts "Max slew check slack: [sta::max_slew_check_slack]"
  puts "Max slew check limit: [sta::max_slew_check_limit]"
}
catch {
  puts "Max cap check slack: [sta::max_capacitance_check_slack]"
  puts "Max cap check limit: [sta::max_capacitance_check_limit]"
}
catch {
  puts "Max fanout check slack: [sta::max_fanout_check_slack]"
  puts "Max fanout check limit: [sta::max_fanout_check_limit]"
}
catch {
  puts "Max slew violation count: [sta::max_slew_violation_count]"
  puts "Max cap violation count: [sta::max_capacitance_violation_count]"
  puts "Max fanout violation count: [sta::max_fanout_violation_count]"
}
puts "PASS: check slack commands"

puts "ALL PASSED"
