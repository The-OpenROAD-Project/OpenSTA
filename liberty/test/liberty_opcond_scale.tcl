# Test operating conditions, scale factors, and multi-corner features.
# Targets:
#   Liberty.cc: findOperatingConditions, defaultOperatingConditions,
#     scaleFactor (all overloads), addScaleFactors, findScaleFactors,
#     inverters(), buffers(), makeCornerMap, setDelayModelType,
#     findLibertyCellsMatching, findLibertyPortsMatching,
#     ocvArcDepth, defaultOcvDerate, supplyVoltage, supplyExists,
#     slewDerateFromLibrary, input/output/slewThresholds,
#     defaultMaxFanout/Slew/Capacitance, defaultFanoutLoad,
#     defaultIntrinsic, defaultPinResistance, footprint
#   LibertyReader.cc: operating_conditions, scale_factors visitor paths
#   EquivCells.cc: EquivCells constructor with map_libs path
source ../../test/helpers.tcl

############################################################
# Read Nangate45 library - has operating conditions
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib

set lib [sta::find_liberty NangateOpenCellLibrary]

############################################################
# Operating conditions queries
############################################################
set op_cond [$lib find_operating_conditions typical]
if { $op_cond == "NULL" } {
  puts "INFO: no operating_conditions named typical"
}

set def_op [$lib default_operating_conditions]
if { $def_op == "NULL" } {
  puts "INFO: no default operating conditions"
}

############################################################
# Set operating conditions and run timing
############################################################
read_verilog ../../sdc/test/sdc_test2.v
link_design sdc_test2

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [all_inputs]
set_output_delay -clock clk1 3.0 [all_outputs]
set_input_transition 0.1 [all_inputs]

set_operating_conditions typical

report_checks -from [get_ports in1] -to [get_ports out1]

############################################################
# Library cell classification queries
# Exercises: inverters(), buffers(), isBuffer(), isInverter()
############################################################
set inv_cell [sta::find_liberty_cell INV_X1]

set buf_cell [sta::find_liberty_cell BUF_X1]

set inv_is_inv [$inv_cell is_inverter]

set inv_is_buf [$inv_cell is_buffer]

set buf_is_buf [$buf_cell is_buffer]

set buf_is_inv [$buf_cell is_inverter]

# Test is_leaf on various cells
set dff_cell [sta::find_liberty_cell DFF_X1]
set dff_leaf [$dff_cell is_leaf]

# Liberty library accessor on cell
set cell_lib [$inv_cell liberty_library]

############################################################
# Pattern matching on liberty cells
# Exercises: findLibertyCellsMatching, findLibertyPortsMatching
############################################################
set inv_matches [$lib find_liberty_cells_matching "INV*" 0 0]

set buf_matches [$lib find_liberty_cells_matching "BUF*" 0 0]

set dff_matches [$lib find_liberty_cells_matching "DFF*" 0 0]

set sdff_matches [$lib find_liberty_cells_matching "SDFF*" 0 0]

set all_matches [$lib find_liberty_cells_matching "*" 0 0]

# Port pattern matching
set inv_port_matches [$inv_cell find_liberty_ports_matching "*" 0 0]

set dff_port_matches [$dff_cell find_liberty_ports_matching "*" 0 0]

############################################################
# Timing arc queries on cells
# Exercises: timingArcSets, timingArcSetCount, hasTimingArcs
############################################################
set inv_arc_sets [$inv_cell timing_arc_sets]

set dff_arc_sets [$dff_cell timing_arc_sets]

# Check timing arc set ports
set clkgate_cell [sta::find_liberty_cell CLKGATETST_X1]
set clkgate_arcs [$clkgate_cell timing_arc_sets]

############################################################
# Find port on liberty cell
# Exercises: findLibertyPort
############################################################
set inv_a [$inv_cell find_liberty_port A]

set inv_zn [$inv_cell find_liberty_port ZN]

set dff_ck [$dff_cell find_liberty_port CK]

set dff_d [$dff_cell find_liberty_port D]

set dff_q [$dff_cell find_liberty_port Q]

############################################################
# Liberty port iterator on cell
# Exercises: LibertyCellPortIterator
############################################################
set port_iter [$inv_cell liberty_port_iterator]
set count 0
while { [$port_iter has_next] } {
  set port [$port_iter next]
  incr count
}
$port_iter finish

set port_iter2 [$dff_cell liberty_port_iterator]
set count2 0
while { [$port_iter2 has_next] } {
  set port [$port_iter2 next]
  incr count2
}
$port_iter2 finish

############################################################
# Wireload queries
# Exercises: findWireload, findWireloadSelection
############################################################
set wl [$lib find_wireload "1K_hvratio_1_1"]
if { $wl == "NULL" } {
  puts "INFO: wireload not found"
}

set wls [$lib find_wireload_selection "WireloadSelection"]
if { $wls == "NULL" } {
  puts "INFO: wireload selection not found"
}

############################################################
# Read Sky130 library - has different features
############################################################
read_liberty ../../test/sky130hd/sky130hd_tt.lib

set sky_lib [sta::find_liberty sky130_fd_sc_hd__tt_025C_1v80]

set sky_op [$sky_lib find_operating_conditions "tt_025C_1v80"]
if { $sky_op == "NULL" } {
  puts "INFO: sky130 no named operating conditions"
}

set sky_def_op [$sky_lib default_operating_conditions]
if { $sky_def_op == "NULL" } {
  puts "INFO: sky130 no default operating conditions"
}

############################################################
# Read fast/slow libraries for multi-corner analysis
# Exercises: makeCornerMap path, setCornerCell, scaleFactor
############################################################
read_liberty ../../test/nangate45/Nangate45_fast.lib

# Read slow too - exercises more corner mapping paths
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__ff_n40C_1v95.lib

read_liberty ../../test/sky130hd/sky130_fd_sc_hd__ss_n40C_1v40.lib

# Report checks exercises multi-library corner paths
report_checks -from [get_ports in1] -to [get_ports out1]

############################################################
# set_timing_derate - exercises OCV paths
############################################################
set_timing_derate -early 0.95
set_timing_derate -late 1.05

report_checks -from [get_ports in1] -to [get_ports out1]

############################################################
# Write liberty for Nangate to exercise all writer paths
############################################################
set outfile [make_result_file liberty_opcond_scale_write.lib]
sta::write_liberty NangateOpenCellLibrary $outfile

set outfile2 [make_result_file liberty_opcond_scale_sky130.lib]
sta::write_liberty sky130_fd_sc_hd__tt_025C_1v80 $outfile2

############################################################
# EquivCells with multiple libraries
# Exercises: EquivCells constructor with map_libs
############################################################
set lib1 [lindex [get_libs NangateOpenCellLibrary] 0]
set lib2 [lindex [get_libs NangateOpenCellLibrary_fast] 0]

sta::make_equiv_cells $lib1

sta::make_equiv_cells $lib2

# Cross-library equiv
set inv_typ [get_lib_cell NangateOpenCellLibrary/INV_X1]
set inv_fast [get_lib_cell NangateOpenCellLibrary_fast/INV_X1]
set result [sta::equiv_cells $inv_typ $inv_fast]

set buf_typ [get_lib_cell NangateOpenCellLibrary/BUF_X1]
set buf_fast [get_lib_cell NangateOpenCellLibrary_fast/BUF_X1]
set result [sta::equiv_cells $buf_typ $buf_fast]

# equiv_cell_ports across libraries
set result [sta::equiv_cell_ports $inv_typ $inv_fast]

# equiv_cell_timing_arcs across libraries
set result [sta::equiv_cell_timing_arcs $inv_typ $inv_fast]

############################################################
# Report check types for max_cap, max_slew, max_fanout
############################################################
report_check_types -max_slew -max_capacitance -max_fanout -verbose
