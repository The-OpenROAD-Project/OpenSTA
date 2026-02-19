# Test CCSN (current source) models and various timing model types.
# Targets:
#   LibertyReader.cc: beginCcs/endCcs, receiver_capacitance groups,
#     timing_type combinations,
#     beginOutputCurrentRise/Fall, visitReceiverCapacitance,
#     polynomial model visitors, ccsn noise model visitors,
#     leakage_power groups, internal_power groups,
#     max_capacitance/max_transition on pins, min_pulse_width
#   TableModel.cc: different table axis variables, GateTableModel,
#     CheckTableModel, 3D tables, receiver model tables
#   TimingArc.cc: timing arc type queries (removal, recovery,
#     three_state_enable, rising_edge, min_pulse_width)
#   Liberty.cc: timing arc set queries, hasTimingArcs, timingArcSets
source ../../test/helpers.tcl

############################################################
# Read ASAP7 CCSN library (CCS models with receiver_capacitance)
############################################################
read_liberty ../../test/asap7_ccsn.lib.gz

# Report cells from CCSN library to exercise CCS model paths
set ccsn_cells [get_lib_cells */*]

foreach cell_obj $ccsn_cells {
  report_lib_cell [get_full_name $cell_obj]
}

############################################################
# Read ASAP7 SEQ library (has setup/hold/recovery/removal arcs)
############################################################
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib

# Report sequential cells which have diverse timing_type values
# DFF cells have setup, hold timing checks
set lib_seq [sta::find_liberty asap7sc7p5t_SEQ_RVT_FF_nldm_220123]
set seq_cells [$lib_seq find_liberty_cells_matching "DFF*" 0 0]

# Report specific cells to exercise different timing types
report_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/DFFHQNx1_ASAP7_75t_R

report_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/DFFHQNx2_ASAP7_75t_R

# Scan DFF cells (scan_in, scan_enable timing arcs)
report_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/SDFHQNx1_ASAP7_75t_R

report_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/SDFHQNx2_ASAP7_75t_R

# ICG cells (clock gating - exercises clock gate timing types)
report_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/ICGx1_ASAP7_75t_R

report_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/ICGx2_ASAP7_75t_R

# Async set/reset cells (recovery/removal timing types)
set async_cells [$lib_seq find_liberty_cells_matching "*ASYNC*" 0 0]

# DFFR cells with reset (recovery/removal)
set dffr_cells [$lib_seq find_liberty_cells_matching "DFFR*" 0 0]
foreach cell_obj $dffr_cells {
  report_lib_cell [get_name $cell_obj]
}

############################################################
# Read ASAP7 SEQ SS corner for different model values
############################################################
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_SS_nldm_220123.lib

############################################################
# Read ASAP7 SIMPLE library (combinational cells)
############################################################
read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz

set simple_lib [sta::find_liberty asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120]
set simple_cells [$simple_lib find_liberty_cells_matching "*" 0 0]

############################################################
# Read ASAP7 AO library (AND-OR complex cells)
############################################################
read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz

set ao_lib [sta::find_liberty asap7sc7p5t_AO_RVT_FF_nldm_211120]
set ao_cells [$ao_lib find_liberty_cells_matching "AO*" 0 0]
foreach c $ao_cells {
  report_lib_cell [get_name $c]
}

############################################################
# Read ASAP7 OA library (OR-AND complex cells)
############################################################
read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz

set oa_lib [sta::find_liberty asap7sc7p5t_OA_RVT_FF_nldm_211120]
set oa_cells [$oa_lib find_liberty_cells_matching "OA*" 0 0]
foreach c $oa_cells {
  report_lib_cell [get_name $c]
}

############################################################
# Read ASAP7 INVBUF library
############################################################
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz

############################################################
# Read libraries from different process nodes
# Exercises different liberty features/syntax in each library
############################################################

# Read IHP SG13G2 library (has tristate, scan, different timing types)
read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p20V_25C.lib

set ihp_lib [sta::find_liberty sg13g2_stdcell_typ_1p20V_25C]
# Report tristate buffer cell (exercises three_state_enable paths)
report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_ebufn_2

# Report scan flip-flop (exercises scan timing paths)
report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_sdfbbp_1

# Report latch cell
report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_dlhq_1

# MUX cell
report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_mux2_1

# Read IHP second PVT corner
read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p50V_25C.lib

############################################################
# Read latch library to exercise latch-specific code
############################################################
read_liberty ../../test/liberty_latch3.lib

############################################################
# Read liberty with backslash-EOL continuation
############################################################
read_liberty ../../test/liberty_backslash_eol.lib

############################################################
# Read liberty with float-as-string values
############################################################
read_liberty ../../test/liberty_float_as_str.lib

############################################################
# Read liberty arcs one2one libraries
############################################################
read_liberty ../../test/liberty_arcs_one2one_1.lib

read_liberty ../../test/liberty_arcs_one2one_2.lib

############################################################
# Read SRAM macro library (exercises macro/memory cells)
############################################################
read_liberty ../../test/gf180mcu_sram.lib.gz

############################################################
# Read ASAP7 SEQ LVT/SLVT (different threshold voltages)
############################################################
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_LVT_FF_nldm_220123.lib

read_liberty ../../test/asap7/asap7sc7p5t_SEQ_SLVT_FF_nldm_220123.lib

############################################################
# Read ASAP7 INVBUF different Vt flavors
############################################################
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_LVT_FF_nldm_220122.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_SLVT_FF_nldm_220122.lib.gz

############################################################
# Write liberty for ASAP7 SEQ
############################################################
set outfile [make_result_file liberty_ccsn_write.lib]
sta::write_liberty asap7sc7p5t_SEQ_RVT_FF_nldm_220123 $outfile

set outfile2 [make_result_file liberty_ccsn_write_ihp.lib]
sta::write_liberty sg13g2_stdcell_typ_1p20V_25C $outfile2
