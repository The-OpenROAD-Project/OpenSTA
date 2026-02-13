# Test Property.cc deeper: LibertyPort drive_resistance and intrinsic_delay
# properties, Instance is_* properties, LibertyCell area/leakage properties,
# path group matching, filterGroupPathMatches, additional timing arc set
# property queries, and TimingArcSet full_name.
# Targets: Property.cc getProperty for LibertyPort (drive_resistance,
#          drive_resistance_min_rise, drive_resistance_max_rise,
#          drive_resistance_min_fall, drive_resistance_max_fall,
#          intrinsic_delay, intrinsic_delay_min_rise, intrinsic_delay_max_rise,
#          intrinsic_delay_min_fall, intrinsic_delay_max_fall,
#          lib_cell, is_clock, is_register_clock, capacitance),
#          Instance (is_hierarchical, is_buffer, is_clock_gate, is_inverter,
#          is_macro, is_memory),
#          Search.cc groupPathMatches, filterGroupPathMatches,
#          PathGroup.cc matchPathEnds
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_multicorner_analysis.v
link_design search_multicorner_analysis

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.5 [get_ports in2]
set_input_delay -clock clk 0.8 [get_ports in3]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 1.5 [get_ports out2]

# Force timing
report_checks -path_delay max > /dev/null
report_checks -path_delay min > /dev/null

############################################################
# LibertyPort drive_resistance properties
############################################################
puts "--- LibertyPort drive_resistance properties ---"
set buf_out [get_lib_pins NangateOpenCellLibrary/BUF_X1/Z]
puts "BUF_X1/Z drive_resistance: [get_property $buf_out drive_resistance]"
puts "BUF_X1/Z drive_resistance_min_rise: [get_property $buf_out drive_resistance_min_rise]"
puts "BUF_X1/Z drive_resistance_max_rise: [get_property $buf_out drive_resistance_max_rise]"
puts "BUF_X1/Z drive_resistance_min_fall: [get_property $buf_out drive_resistance_min_fall]"
puts "BUF_X1/Z drive_resistance_max_fall: [get_property $buf_out drive_resistance_max_fall]"
puts "PASS: drive_resistance"

puts "--- More drive_resistance on different cells ---"
set and_out [get_lib_pins NangateOpenCellLibrary/AND2_X1/ZN]
puts "AND2_X1/ZN drive_resistance: [get_property $and_out drive_resistance]"
puts "AND2_X1/ZN drive_resistance_min_rise: [get_property $and_out drive_resistance_min_rise]"
puts "AND2_X1/ZN drive_resistance_max_fall: [get_property $and_out drive_resistance_max_fall]"

set inv_out [get_lib_pins NangateOpenCellLibrary/INV_X1/ZN]
puts "INV_X1/ZN drive_resistance: [get_property $inv_out drive_resistance]"
puts "INV_X1/ZN drive_resistance_min_rise: [get_property $inv_out drive_resistance_min_rise]"
puts "INV_X1/ZN drive_resistance_max_fall: [get_property $inv_out drive_resistance_max_fall]"
puts "PASS: drive_resistance multi"

############################################################
# LibertyPort intrinsic_delay properties
############################################################
puts "--- LibertyPort intrinsic_delay properties ---"
puts "BUF_X1/Z intrinsic_delay: [get_property $buf_out intrinsic_delay]"
puts "BUF_X1/Z intrinsic_delay_min_rise: [get_property $buf_out intrinsic_delay_min_rise]"
puts "BUF_X1/Z intrinsic_delay_max_rise: [get_property $buf_out intrinsic_delay_max_rise]"
puts "BUF_X1/Z intrinsic_delay_min_fall: [get_property $buf_out intrinsic_delay_min_fall]"
puts "BUF_X1/Z intrinsic_delay_max_fall: [get_property $buf_out intrinsic_delay_max_fall]"
puts "PASS: intrinsic_delay BUF"

puts "--- intrinsic_delay on AND ---"
puts "AND2_X1/ZN intrinsic_delay: [get_property $and_out intrinsic_delay]"
puts "AND2_X1/ZN intrinsic_delay_min_rise: [get_property $and_out intrinsic_delay_min_rise]"
puts "AND2_X1/ZN intrinsic_delay_max_rise: [get_property $and_out intrinsic_delay_max_rise]"
puts "AND2_X1/ZN intrinsic_delay_min_fall: [get_property $and_out intrinsic_delay_min_fall]"
puts "AND2_X1/ZN intrinsic_delay_max_fall: [get_property $and_out intrinsic_delay_max_fall]"
puts "PASS: intrinsic_delay AND"

puts "--- intrinsic_delay on INV ---"
puts "INV_X1/ZN intrinsic_delay: [get_property $inv_out intrinsic_delay]"
puts "INV_X1/ZN intrinsic_delay_min_rise: [get_property $inv_out intrinsic_delay_min_rise]"
puts "INV_X1/ZN intrinsic_delay_max_fall: [get_property $inv_out intrinsic_delay_max_fall]"
puts "PASS: intrinsic_delay INV"

############################################################
# LibertyPort lib_cell, is_clock, is_register_clock, capacitance
############################################################
puts "--- LibertyPort lib_cell and clock properties ---"
set buf_in [get_lib_pins NangateOpenCellLibrary/BUF_X1/A]
puts "BUF_X1/A capacitance: [get_property $buf_in capacitance]"
set buf_lc [get_property $buf_in lib_cell]
puts "BUF_X1/A lib_cell: [get_name $buf_lc]"
puts "BUF_X1/A is_clock: [get_property $buf_in is_clock]"
puts "BUF_X1/A is_register_clock: [get_property $buf_in is_register_clock]"

set dff_ck [get_lib_pins NangateOpenCellLibrary/DFF_X1/CK]
puts "DFF_X1/CK is_clock: [get_property $dff_ck is_clock]"
puts "DFF_X1/CK is_register_clock: [get_property $dff_ck is_register_clock]"
puts "DFF_X1/CK capacitance: [get_property $dff_ck capacitance]"
set dff_lc [get_property $dff_ck lib_cell]
puts "DFF_X1/CK lib_cell: [get_name $dff_lc]"

set dff_d [get_lib_pins NangateOpenCellLibrary/DFF_X1/D]
puts "DFF_X1/D is_clock: [get_property $dff_d is_clock]"
puts "DFF_X1/D is_register_clock: [get_property $dff_d is_register_clock]"
puts "DFF_X1/D capacitance: [get_property $dff_d capacitance]"
puts "PASS: lib_cell/is_clock/capacitance"

############################################################
# Instance is_* properties
############################################################
puts "--- Instance is_* properties ---"
set buf_inst [get_cells buf1]
puts "buf1 is_buffer: [get_property $buf_inst is_buffer]"
puts "buf1 is_inverter: [get_property $buf_inst is_inverter]"
puts "buf1 is_clock_gate: [get_property $buf_inst is_clock_gate]"
puts "buf1 is_macro: [get_property $buf_inst is_macro]"
puts "buf1 is_memory: [get_property $buf_inst is_memory]"
puts "buf1 is_hierarchical: [get_property $buf_inst is_hierarchical]"

set inv_inst [get_cells inv1]
puts "inv1 is_buffer: [get_property $inv_inst is_buffer]"
puts "inv1 is_inverter: [get_property $inv_inst is_inverter]"
puts "inv1 is_clock_gate: [get_property $inv_inst is_clock_gate]"

set and_inst [get_cells and1]
puts "and1 is_buffer: [get_property $and_inst is_buffer]"
puts "and1 is_inverter: [get_property $and_inst is_inverter]"

set reg_inst [get_cells reg1]
puts "reg1 is_buffer: [get_property $reg_inst is_buffer]"
puts "reg1 is_inverter: [get_property $reg_inst is_inverter]"
puts "reg1 is_macro: [get_property $reg_inst is_macro]"
puts "reg1 is_memory: [get_property $reg_inst is_memory]"
puts "PASS: instance is_* properties"

############################################################
# LibertyCell area and leakage power
############################################################
puts "--- LibertyCell area and leakage ---"
set dff_cell [get_lib_cells NangateOpenCellLibrary/DFF_X1]
catch { puts "DFF_X1 area: [get_property $dff_cell area]" }
catch { puts "DFF_X1 cell_leakage_power: [get_property $dff_cell cell_leakage_power]" }
set buf_cell [get_lib_cells NangateOpenCellLibrary/BUF_X1]
catch { puts "BUF_X1 area: [get_property $buf_cell area]" }
catch { puts "BUF_X1 cell_leakage_power: [get_property $buf_cell cell_leakage_power]" }
set inv_cell [get_lib_cells NangateOpenCellLibrary/INV_X1]
catch { puts "INV_X1 area: [get_property $inv_cell area]" }
set and_cell [get_lib_cells NangateOpenCellLibrary/AND2_X1]
catch { puts "AND2_X1 area: [get_property $and_cell area]" }
puts "PASS: cell area/leakage"

############################################################
# Path group matching: group_path -name with -from and -through
# This exercises Search.cc groupPathMatches, filterGroupPathMatches
############################################################
puts "--- group_path matching ---"
group_path -name input_grp -from [get_ports {in1 in2 in3}]
group_path -name reg2reg_grp -from [get_pins reg1/CK] -to [get_pins reg2/D]
group_path -name output_grp -to [get_ports {out1 out2}]
group_path -name through_grp -through [get_pins inv1/ZN]

puts "--- report_checks with groups ---"
report_checks -path_delay max
puts "PASS: report_checks with groups"

puts "--- find_timing_paths with group_path ---"
set paths [find_timing_paths -path_delay max -group_path_count 20 -endpoint_path_count 10]
puts "Found [llength $paths] paths with groups"
foreach pe $paths {
  catch {
    set pg [$pe path_group]
    puts "  [get_full_name [$pe pin]] group=[$pg name] slack=[$pe slack]"
  }
}
puts "PASS: paths with groups"

puts "--- find_timing_paths with min paths and groups ---"
set paths_min [find_timing_paths -path_delay min -group_path_count 20 -endpoint_path_count 10]
puts "Found [llength $paths_min] min paths with groups"
foreach pe $paths_min {
  catch {
    set pg [$pe path_group]
    puts "  [get_full_name [$pe pin]] group=[$pg name] slack=[$pe slack]"
  }
}
puts "PASS: min paths with groups"

############################################################
# path_group_names and is_path_group_name
############################################################
puts "--- path_group_names ---"
set group_names [sta::path_group_names]
puts "Path group names: $group_names"
catch { puts "input_grp is group: [sta::is_path_group_name input_grp]" }
catch { puts "nonexistent is group: [sta::is_path_group_name nonexistent_grp]" }
puts "PASS: path_group_names"

############################################################
# TimingArcSet properties on different cell types
############################################################
puts "--- TimingArcSet properties on DFF_X1 ---"
set dff_cell2 [get_lib_cells NangateOpenCellLibrary/DFF_X1]
set arcsets [$dff_cell2 timing_arc_sets]
foreach arcset $arcsets {
  set arcname [sta::timing_arc_property $arcset full_name]
  set arcname2 [sta::timing_arc_property $arcset name]
  puts "DFF_X1 arc: $arcname / $arcname2"
}
puts "PASS: DFF_X1 arc set properties"

puts "--- TimingArcSet properties on DFFR_X1 ---"
set dffr_cell [get_lib_cells NangateOpenCellLibrary/DFFR_X1]
set arcsets_r [$dffr_cell timing_arc_sets]
foreach arcset $arcsets_r {
  set arcname [sta::timing_arc_property $arcset full_name]
  puts "DFFR_X1 arc: $arcname"
}
puts "PASS: DFFR_X1 arc set properties"

puts "--- TimingArcSet properties on OR2_X1 ---"
set or_cell [get_lib_cells NangateOpenCellLibrary/OR2_X1]
set arcsets_o [$or_cell timing_arc_sets]
foreach arcset $arcsets_o {
  set arcname [sta::timing_arc_property $arcset full_name]
  set arcname2 [sta::timing_arc_property $arcset name]
  puts "OR2_X1 arc: $arcname / $arcname2"
}
puts "PASS: OR2_X1 arc set properties"

############################################################
# Pin property: is_hierarchical, is_port
############################################################
puts "--- Pin is_hierarchical/is_port ---"
set p1 [get_pins reg1/D]
puts "reg1/D is_hierarchical: [get_property $p1 is_hierarchical]"
puts "reg1/D is_port: [get_property $p1 is_port]"
puts "PASS: pin is_hierarchical/is_port"

############################################################
# LibertyPort direction on different port types
############################################################
puts "--- LibertyPort direction varieties ---"
set dff_q [get_lib_pins NangateOpenCellLibrary/DFF_X1/Q]
puts "DFF_X1/Q direction: [get_property $dff_q direction]"
set dff_d_lp [get_lib_pins NangateOpenCellLibrary/DFF_X1/D]
puts "DFF_X1/D direction: [get_property $dff_d_lp direction]"
set dff_ck_lp [get_lib_pins NangateOpenCellLibrary/DFF_X1/CK]
puts "DFF_X1/CK direction: [get_property $dff_ck_lp direction]"
puts "PASS: liberty port directions"

############################################################
# Unknown property error handling for various types
############################################################
puts "--- Unknown property errors ---"
catch { get_property [get_lib_pins NangateOpenCellLibrary/BUF_X1/Z] nonexistent_prop } err1
puts "LibertyPort unknown: [string range $err1 0 40]"
catch { get_property [get_cells reg1] nonexistent_prop } err2
puts "Instance unknown: [string range $err2 0 40]"
catch { get_property [get_clocks clk] nonexistent_prop } err3
puts "Clock unknown: [string range $err3 0 40]"
catch { get_property [get_lib_cells NangateOpenCellLibrary/BUF_X1] nonexistent_prop } err4
puts "LibertyCell unknown: [string range $err4 0 40]"
catch { get_property [get_libs NangateOpenCellLibrary] nonexistent_prop } err5
puts "Library unknown: [string range $err5 0 40]"
puts "PASS: unknown property errors"

puts "ALL PASSED"
