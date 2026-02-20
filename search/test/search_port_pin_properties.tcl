# Test Property.cc port/pin property getters that are uncovered:
# - pinSlack(Pin, MinMax) and pinSlack(Pin, RiseFall, MinMax)
# - pinArrival(Pin, RiseFall, MinMax)
# - portSlack and portSlew through get_property on Port objects
# - PropertyValue constructors for various types
# - Net property getters
# - Cell property getters (ref_name, liberty_cell, etc.)
# Covers: Property.cc pinSlack, pinArrival, portSlack, portSlew,
#         PropertyValue ctors, getProperty for Net/Port/Instance/Clock
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.5 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

# Force timing
report_checks -path_delay max > /dev/null
report_checks -path_delay min > /dev/null

puts "--- Port slack properties (portSlack calls pinSlack) ---"
set in_port [get_ports in1]
puts "in1 slack_max: [get_property $in_port slack_max]"
puts "in1 slack_max_rise: [get_property $in_port slack_max_rise]"
puts "in1 slack_max_fall: [get_property $in_port slack_max_fall]"
puts "in1 slack_min: [get_property $in_port slack_min]"
puts "in1 slack_min_rise: [get_property $in_port slack_min_rise]"
puts "in1 slack_min_fall: [get_property $in_port slack_min_fall]"

puts "--- Port slew properties (portSlew calls pinSlew) ---"
puts "in1 slew_max: [get_property $in_port slew_max]"
puts "in1 slew_max_rise: [get_property $in_port slew_max_rise]"
puts "in1 slew_max_fall: [get_property $in_port slew_max_fall]"
puts "in1 slew_min: [get_property $in_port slew_min]"
puts "in1 slew_min_rise: [get_property $in_port slew_min_rise]"
puts "in1 slew_min_fall: [get_property $in_port slew_min_fall]"

puts "--- Output port slack/slew ---"
set out_port [get_ports out1]
puts "out1 slack_max: [get_property $out_port slack_max]"
puts "out1 slack_max_rise: [get_property $out_port slack_max_rise]"
puts "out1 slack_max_fall: [get_property $out_port slack_max_fall]"
puts "out1 slack_min: [get_property $out_port slack_min]"
puts "out1 slack_min_rise: [get_property $out_port slack_min_rise]"
puts "out1 slack_min_fall: [get_property $out_port slack_min_fall]"
puts "out1 slew_max: [get_property $out_port slew_max]"
puts "out1 slew_min: [get_property $out_port slew_min]"

puts "--- Port direction and liberty_port ---"
puts "in1 direction: [get_property $in_port direction]"
puts "in1 port_direction: [get_property $in_port port_direction]"
set lp [get_property $in_port liberty_port]
if { $lp != "" && $lp != "NULL" } {
  puts "in1 liberty_port: [get_name $lp]"
} else {
  puts "in1 liberty_port: none"
}
puts "out1 direction: [get_property $out_port direction]"

puts "--- Port activity ---"
puts "in1 activity: [get_property $in_port activity]"
puts "out1 activity: [get_property $out_port activity]"

puts "--- Pin slack (via direct pin property) ---"
set dpin [get_pins reg1/D]
puts "reg1/D slack_max: [get_property $dpin slack_max]"
puts "reg1/D slack_max_rise: [get_property $dpin slack_max_rise]"
puts "reg1/D slack_max_fall: [get_property $dpin slack_max_fall]"
puts "reg1/D slack_min: [get_property $dpin slack_min]"
puts "reg1/D slack_min_rise: [get_property $dpin slack_min_rise]"
puts "reg1/D slack_min_fall: [get_property $dpin slack_min_fall]"

puts "--- Pin slew ---"
puts "reg1/D slew_max: [get_property $dpin slew_max]"
puts "reg1/D slew_max_rise: [get_property $dpin slew_max_rise]"
puts "reg1/D slew_max_fall: [get_property $dpin slew_max_fall]"
puts "reg1/D slew_min: [get_property $dpin slew_min]"
puts "reg1/D slew_min_rise: [get_property $dpin slew_min_rise]"
puts "reg1/D slew_min_fall: [get_property $dpin slew_min_fall]"

puts "--- Pin arrival ---"
puts "reg1/D arrival_max_rise: [get_property $dpin arrival_max_rise]"
puts "reg1/D arrival_max_fall: [get_property $dpin arrival_max_fall]"
puts "reg1/D arrival_min_rise: [get_property $dpin arrival_min_rise]"
puts "reg1/D arrival_min_fall: [get_property $dpin arrival_min_fall]"

puts "--- Pin activity ---"
puts "reg1/D activity: [get_property $dpin activity]"

puts "--- Pin clock properties ---"
set ckpin [get_pins reg1/CK]
puts "reg1/CK is_clock: [get_property $ckpin is_clock]"
puts "reg1/CK is_register_clock: [get_property $ckpin is_register_clock]"
set ck_clocks [get_property $ckpin clocks]
puts "reg1/CK clocks: [llength $ck_clocks]"
set ck_domains [get_property $ckpin clock_domains]
puts "reg1/CK clock_domains: [llength $ck_domains]"

puts "--- Net properties ---"
set net1 [get_nets n1]
puts "n1 name: [get_property $net1 name]"
puts "n1 full_name: [get_property $net1 full_name]"
# Test unknown property error for net
catch {
  get_property $net1 nonexistent_net_property
} net_err
puts "Unknown net property caught: [string range $net_err 0 30]"

puts "--- Instance properties ---"
set inst [get_cells reg1]
puts "reg1 name: [get_property $inst name]"
puts "reg1 full_name: [get_property $inst full_name]"
puts "reg1 ref_name: [get_property $inst ref_name]"
set inst_cell [get_property $inst cell]
puts "reg1 cell: [get_name $inst_cell]"
set inst_lcell [get_property $inst liberty_cell]
puts "reg1 liberty_cell: [get_name $inst_lcell]"

puts "--- Clock properties ---"
set clk_obj [get_clocks clk]
puts "clk name: [get_property $clk_obj name]"
puts "clk period: [get_property $clk_obj period]"
puts "clk is_generated: [get_property $clk_obj is_generated]"
puts "clk is_virtual: [get_property $clk_obj is_virtual]"
puts "clk is_propagated: [get_property $clk_obj is_propagated]"
set clk_sources [get_property $clk_obj sources]
puts "clk sources: [llength $clk_sources]"

puts "--- LibertyCell properties ---"
set dff_cell [get_lib_cells NangateOpenCellLibrary/DFF_X1]
puts "DFF_X1 name: [get_property $dff_cell name]"
puts "DFF_X1 full_name: [get_property $dff_cell full_name]"
puts "DFF_X1 base_name: [get_property $dff_cell base_name]"
puts "DFF_X1 is_buffer: [get_property $dff_cell is_buffer]"
set dff_lib [get_property $dff_cell library]
puts "DFF_X1 library: [get_name $dff_lib]"
puts "DFF_X1 area: [get_property $dff_cell area]"
catch { puts "DFF_X1 leakage: [get_property $dff_cell cell_leakage_power]" }

puts "--- LibertyPort properties ---"
set lp_d [get_lib_pins NangateOpenCellLibrary/DFF_X1/D]
puts "DFF_X1/D name: [get_property $lp_d name]"
puts "DFF_X1/D full_name: [get_property $lp_d full_name]"
puts "DFF_X1/D direction: [get_property $lp_d direction]"
puts "DFF_X1/D capacitance: [get_property $lp_d capacitance]"
puts "DFF_X1/D is_clock: [get_property $lp_d is_clock]"
catch { puts "DFF_X1/D is_register_clock: [get_property $lp_d is_register_clock]" }
set lp_ck [get_lib_pins NangateOpenCellLibrary/DFF_X1/CK]
puts "DFF_X1/CK is_clock: [get_property $lp_ck is_clock]"
catch { puts "DFF_X1/CK is_register_clock: [get_property $lp_ck is_register_clock]" }

puts "--- Library properties ---"
set lib [get_libs NangateOpenCellLibrary]
puts "lib name: [get_property $lib name]"
puts "lib full_name: [get_property $lib full_name]"
puts "lib filename: [get_property $lib filename]"

puts "--- Edge properties ---"
set edges [get_timing_edges -from [get_pins and1/A1] -to [get_pins and1/ZN]]
foreach edge $edges {
  puts "edge full_name: [get_property $edge full_name]"
  puts "edge delay_min_rise: [get_property $edge delay_min_rise]"
  puts "edge delay_min_fall: [get_property $edge delay_min_fall]"
  puts "edge delay_max_rise: [get_property $edge delay_max_rise]"
  puts "edge delay_max_fall: [get_property $edge delay_max_fall]"
  puts "edge sense: [get_property $edge sense]"
  set efp [get_property $edge from_pin]
  puts "edge from_pin: [get_full_name $efp]"
  set etp [get_property $edge to_pin]
  puts "edge to_pin: [get_full_name $etp]"
  break
}

puts "--- TimingArcSet property ---"
set and_cell [get_lib_cells NangateOpenCellLibrary/AND2_X1]
set arcsets [$and_cell timing_arc_sets]
foreach arcset $arcsets {
  set arcprop [sta::timing_arc_property $arcset full_name]
  puts "arc_set full_name: $arcprop"
  set arcprop2 [sta::timing_arc_property $arcset name]
  puts "arc_set name: $arcprop2"
}

puts "--- PathEnd properties ---"
set path_ends [find_timing_paths -path_delay max -endpoint_path_count 5]
foreach pe $path_ends {
  set sp [get_property $pe startpoint]
  puts "pathend startpoint: [get_full_name $sp]"
  set ep [get_property $pe endpoint]
  puts "pathend endpoint: [get_full_name $ep]"
  set sc [get_property $pe startpoint_clock]
  puts "pathend startpoint_clock: [get_name $sc]"
  set ec [get_property $pe endpoint_clock]
  puts "pathend endpoint_clock: [get_name $ec]"
  puts "pathend slack: [get_property $pe slack]"
  set pts [get_property $pe points]
  puts "pathend points count: [llength $pts]"
  break
}

puts "--- Path properties ---"
set path_ends2 [find_timing_paths -path_delay max]
foreach pe $path_ends2 {
  set p [$pe path]
  set ppin [get_property $p pin]
  puts "path pin: [get_full_name $ppin]"
  puts "path arrival: [get_property $p arrival]"
  puts "path required: [get_property $p required]"
  puts "path slack: [get_property $p slack]"
  break
}

puts "--- Unknown property error handling ---"
catch {
  get_property $in_port nonexistent_property
} result
puts "Unknown port property caught: [string range $result 0 30]"
catch {
  get_property $dpin nonexistent_property
} result2
puts "Unknown pin property caught: [string range $result2 0 30]"
