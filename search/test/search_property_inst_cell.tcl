# Test Property.cc: instance properties (is_buffer, is_inverter, is_macro,
# is_memory, is_clock_gate, is_hierarchical, liberty_cell, cell),
# LibertyCell properties (is_inverter, is_memory, dont_use, area),
# Pin properties (is_port, is_hierarchical, direction, pin_direction),
# Net properties, Port properties (liberty_port, activity, slack/slew variants),
# PathEnd property "points", Clock property "is_propagated",
# LibertyLibrary property "filename",
# Cell property full_name/library/filename.
# Targets: Property.cc getProperty for Instance, LibertyCell, Pin, Net, Port,
#          Cell, LibertyLibrary, Clock (is_propagated), PathEnd (points).
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

# Run timing so arrivals/requireds are computed
report_checks -path_delay max > /dev/null
report_checks -path_delay min > /dev/null

############################################################
# Instance properties: is_buffer, is_inverter, is_clock_gate,
#                      is_macro, is_memory, is_hierarchical,
#                      liberty_cell, cell
############################################################
puts "--- Instance is_buffer/is_inverter ---"
set buf_inst [get_cells buf1]
puts "buf1 is_buffer: [get_property $buf_inst is_buffer]"
puts "buf1 is_inverter: [get_property $buf_inst is_inverter]"
puts "buf1 is_clock_gate: [get_property $buf_inst is_clock_gate]"
puts "buf1 is_macro: [get_property $buf_inst is_macro]"
puts "buf1 is_memory: [get_property $buf_inst is_memory]"
puts "buf1 is_hierarchical: [get_property $buf_inst is_hierarchical]"
set buf_lc [get_property $buf_inst liberty_cell]
puts "buf1 liberty_cell: [get_name $buf_lc]"
set buf_cell [get_property $buf_inst cell]
puts "buf1 cell: [get_name $buf_cell]"
puts "PASS: instance properties"

puts "--- Instance properties for and gate ---"
set and_inst [get_cells and1]
puts "and1 is_buffer: [get_property $and_inst is_buffer]"
puts "and1 is_inverter: [get_property $and_inst is_inverter]"
puts "and1 is_clock_gate: [get_property $and_inst is_clock_gate]"
puts "and1 is_macro: [get_property $and_inst is_macro]"
puts "and1 is_memory: [get_property $and_inst is_memory]"
puts "PASS: and instance properties"

puts "--- Instance properties for register ---"
set reg_inst [get_cells reg1]
puts "reg1 is_buffer: [get_property $reg_inst is_buffer]"
puts "reg1 is_inverter: [get_property $reg_inst is_inverter]"
puts "reg1 is_clock_gate: [get_property $reg_inst is_clock_gate]"
puts "reg1 is_macro: [get_property $reg_inst is_macro]"
puts "reg1 is_memory: [get_property $reg_inst is_memory]"
puts "PASS: reg instance properties"

############################################################
# LibertyCell properties: is_inverter, is_memory, dont_use, area
############################################################
puts "--- LibertyCell properties ---"
set inv_cell [get_lib_cells NangateOpenCellLibrary/INV_X1]
puts "INV_X1 is_buffer: [get_property $inv_cell is_buffer]"
puts "INV_X1 is_inverter: [get_property $inv_cell is_inverter]"
puts "INV_X1 is_memory: [get_property $inv_cell is_memory]"
puts "INV_X1 dont_use: [get_property $inv_cell dont_use]"
puts "INV_X1 area: [get_property $inv_cell area]"
puts "PASS: INV_X1 liberty cell properties"

set buf_cell [get_lib_cells NangateOpenCellLibrary/BUF_X1]
puts "BUF_X1 is_buffer: [get_property $buf_cell is_buffer]"
puts "BUF_X1 is_inverter: [get_property $buf_cell is_inverter]"
puts "BUF_X1 is_memory: [get_property $buf_cell is_memory]"
puts "BUF_X1 dont_use: [get_property $buf_cell dont_use]"
puts "BUF_X1 area: [get_property $buf_cell area]"
puts "PASS: BUF_X1 liberty cell properties"

set dff_cell [get_lib_cells NangateOpenCellLibrary/DFF_X1]
puts "DFF_X1 is_buffer: [get_property $dff_cell is_buffer]"
puts "DFF_X1 is_inverter: [get_property $dff_cell is_inverter]"
puts "DFF_X1 is_memory: [get_property $dff_cell is_memory]"
puts "DFF_X1 dont_use: [get_property $dff_cell dont_use]"
puts "DFF_X1 area: [get_property $dff_cell area]"
puts "DFF_X1 filename: [get_property $dff_cell filename]"
set dff_lib [get_property $dff_cell library]
puts "DFF_X1 library: [get_name $dff_lib]"
puts "PASS: DFF_X1 liberty cell properties"

############################################################
# Cell properties: full_name, library, filename
############################################################
puts "--- Cell properties ---"
set cell_ref [get_property [get_cells buf1] cell]
puts "cell name: [get_property $cell_ref name]"
puts "cell full_name: [get_property $cell_ref full_name]"
set cell_lib [get_property $cell_ref library]
puts "cell library: [get_name $cell_lib]"
puts "cell filename: [get_property $cell_ref filename]"
puts "PASS: cell properties"

############################################################
# LibertyLibrary properties: filename
############################################################
puts "--- LibertyLibrary filename ---"
set llib [get_libs NangateOpenCellLibrary]
puts "lib name: [get_property $llib name]"
puts "lib full_name: [get_property $llib full_name]"
puts "lib filename: [get_property $llib filename]"
puts "PASS: liberty library properties"

############################################################
# Pin properties: is_port, direction, pin_direction, is_hierarchical
############################################################
puts "--- Pin is_port, direction ---"
set buf_a_pin [get_pins buf1/A]
puts "buf1/A is_port: [get_property $buf_a_pin is_port]"
puts "buf1/A direction: [get_property $buf_a_pin direction]"
puts "buf1/A pin_direction: [get_property $buf_a_pin pin_direction]"
puts "buf1/A is_hierarchical: [get_property $buf_a_pin is_hierarchical]"
puts "buf1/A full_name: [get_property $buf_a_pin full_name]"
puts "buf1/A name: [get_property $buf_a_pin name]"
puts "buf1/A lib_pin_name: [get_property $buf_a_pin lib_pin_name]"

set buf_z_pin [get_pins buf1/Z]
puts "buf1/Z is_port: [get_property $buf_z_pin is_port]"
puts "buf1/Z direction: [get_property $buf_z_pin direction]"
puts "buf1/Z pin_direction: [get_property $buf_z_pin pin_direction]"
puts "PASS: pin direction properties"

puts "--- Pin is_clock, is_register_clock ---"
set ck_pin [get_pins reg1/CK]
puts "reg1/CK is_clock: [get_property $ck_pin is_clock]"
puts "reg1/CK is_register_clock: [get_property $ck_pin is_register_clock]"
set d_pin [get_pins reg1/D]
puts "reg1/D is_clock: [get_property $d_pin is_clock]"
puts "reg1/D is_register_clock: [get_property $d_pin is_register_clock]"
puts "PASS: pin clock properties"

puts "--- Pin activity ---"
set p_activity [get_property [get_pins buf1/A] activity]
puts "buf1/A activity: $p_activity"
puts "PASS: pin activity"

############################################################
# Port properties: liberty_port, activity, slack/slew variants
############################################################
puts "--- Port liberty_port ---"
set in_port [get_ports in1]
set lport [get_property $in_port liberty_port]
puts "in1 liberty_port: $lport"
puts "PASS: port liberty_port"

puts "--- Port activity ---"
set p_act [get_property $in_port activity]
puts "in1 activity: $p_act"
puts "PASS: port activity"

puts "--- Port slack variants ---"
set out_port [get_ports out1]
puts "out1 slack_max: [get_property $out_port slack_max]"
puts "out1 slack_max_rise: [get_property $out_port slack_max_rise]"
puts "out1 slack_max_fall: [get_property $out_port slack_max_fall]"
puts "out1 slack_min: [get_property $out_port slack_min]"
puts "out1 slack_min_rise: [get_property $out_port slack_min_rise]"
puts "out1 slack_min_fall: [get_property $out_port slack_min_fall]"
puts "PASS: port slack variants"

puts "--- Port slew variants ---"
puts "in1 slew_max: [get_property $in_port slew_max]"
puts "in1 slew_max_rise: [get_property $in_port slew_max_rise]"
puts "in1 slew_max_fall: [get_property $in_port slew_max_fall]"
puts "in1 slew_min: [get_property $in_port slew_min]"
puts "in1 slew_min_rise: [get_property $in_port slew_min_rise]"
puts "in1 slew_min_fall: [get_property $in_port slew_min_fall]"
puts "PASS: port slew variants"

############################################################
# Clock property: is_propagated
############################################################
puts "--- Clock is_propagated ---"
set myclk [get_clocks clk]
puts "clk is_propagated: [get_property $myclk is_propagated]"
puts "clk is_virtual: [get_property $myclk is_virtual]"
puts "clk is_generated: [get_property $myclk is_generated]"
puts "clk period: [get_property $myclk period]"
puts "clk name: [get_property $myclk name]"
puts "clk full_name: [get_property $myclk full_name]"
set clk_srcs [get_property $myclk sources]
puts "clk sources: [llength $clk_srcs]"
puts "PASS: clock properties"

puts "--- Propagated clock property ---"
set_propagated_clock [get_clocks clk]
report_checks -path_delay max > /dev/null
puts "clk is_propagated (after set): [get_property [get_clocks clk] is_propagated]"
unset_propagated_clock [get_clocks clk]
puts "PASS: propagated clock"

############################################################
# PathEnd property: points
############################################################
puts "--- PathEnd points property ---"
set paths [find_timing_paths -path_delay max -endpoint_path_count 3]
foreach pe $paths {
  set points [get_property $pe points]
  puts "PathEnd points count: [llength $points]"
  foreach pt $points {
    puts "  point pin: [get_full_name [get_property $pt pin]]"
    puts "  point arrival: [get_property $pt arrival]"
  }
  break
}
puts "PASS: PathEnd points"

############################################################
# Edge properties - is_disabled_cond checking
############################################################
puts "--- Edge disabled properties ---"
set edges2 [get_timing_edges -from [get_pins buf1/A] -to [get_pins buf1/Z]]
foreach edge $edges2 {
  puts "edge sense: [get_property $edge sense]"
  puts "edge from_pin: [get_full_name [get_property $edge from_pin]]"
  puts "edge to_pin: [get_full_name [get_property $edge to_pin]]"
  puts "edge delay_min_rise: [get_property $edge delay_min_rise]"
  puts "edge delay_max_rise: [get_property $edge delay_max_rise]"
  puts "edge delay_min_fall: [get_property $edge delay_min_fall]"
  puts "edge delay_max_fall: [get_property $edge delay_max_fall]"
  break
}
puts "PASS: edge disabled properties"

############################################################
# LibertyPort properties: drive_resistance variants,
# intrinsic_delay variants, capacitance, is_clock, is_register_clock
############################################################
puts "--- LibertyPort drive_resistance/intrinsic_delay ---"
set lp_z [get_lib_pins NangateOpenCellLibrary/BUF_X1/Z]
puts "BUF_X1/Z drive_resistance: [get_property $lp_z drive_resistance]"
puts "BUF_X1/Z drive_resistance_min_rise: [get_property $lp_z drive_resistance_min_rise]"
puts "BUF_X1/Z drive_resistance_max_rise: [get_property $lp_z drive_resistance_max_rise]"
puts "BUF_X1/Z drive_resistance_min_fall: [get_property $lp_z drive_resistance_min_fall]"
puts "BUF_X1/Z drive_resistance_max_fall: [get_property $lp_z drive_resistance_max_fall]"
puts "BUF_X1/Z intrinsic_delay: [get_property $lp_z intrinsic_delay]"
puts "BUF_X1/Z intrinsic_delay_min_rise: [get_property $lp_z intrinsic_delay_min_rise]"
puts "BUF_X1/Z intrinsic_delay_max_rise: [get_property $lp_z intrinsic_delay_max_rise]"
puts "BUF_X1/Z intrinsic_delay_min_fall: [get_property $lp_z intrinsic_delay_min_fall]"
puts "BUF_X1/Z intrinsic_delay_max_fall: [get_property $lp_z intrinsic_delay_max_fall]"
puts "BUF_X1/Z capacitance: [get_property $lp_z capacitance]"
puts "BUF_X1/Z is_clock: [get_property $lp_z is_clock]"
puts "BUF_X1/Z is_register_clock: [get_property $lp_z is_register_clock]"
puts "BUF_X1/Z direction: [get_property $lp_z direction]"
puts "BUF_X1/Z port_direction: [get_property $lp_z port_direction]"
set lp_cell [get_property $lp_z lib_cell]
puts "BUF_X1/Z lib_cell: [get_name $lp_cell]"
puts "PASS: liberty port properties"

puts "--- LibertyPort for clock pin ---"
set lp_ck [get_lib_pins NangateOpenCellLibrary/DFF_X1/CK]
puts "DFF_X1/CK is_clock: [get_property $lp_ck is_clock]"
puts "DFF_X1/CK is_register_clock: [get_property $lp_ck is_register_clock]"
puts "DFF_X1/CK direction: [get_property $lp_ck direction]"
puts "PASS: DFF CK liberty port"

set lp_d [get_lib_pins NangateOpenCellLibrary/DFF_X1/D]
puts "DFF_X1/D is_clock: [get_property $lp_d is_clock]"
puts "DFF_X1/D is_register_clock: [get_property $lp_d is_register_clock]"
puts "PASS: DFF D liberty port"

puts "ALL PASSED"
