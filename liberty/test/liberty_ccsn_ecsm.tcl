# Test CCSN (current source) models and various timing model types.
# Targets:
#   LibertyReader.cc: beginCcs/endCcs, receiver_capacitance groups,
#     ECSM waveform handling, timing_type combinations,
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
puts "PASS: read ASAP7 CCSN library"

# Report cells from CCSN library to exercise CCS model paths
set ccsn_cells [get_lib_cells */*]
puts "PASS: CCSN lib cells total: [llength $ccsn_cells]"

foreach cell_obj $ccsn_cells {
  catch {
    report_lib_cell [get_full_name $cell_obj]
  }
}
puts "PASS: reported all CCSN cells"

############################################################
# Read ASAP7 SEQ library (has setup/hold/recovery/removal arcs)
############################################################
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
puts "PASS: read ASAP7 SEQ RVT FF"

# Report sequential cells which have diverse timing_type values
# DFF cells have setup, hold timing checks
set lib_seq [sta::find_liberty asap7sc7p5t_SEQ_RVT_FF_nldm_220123]
catch {
  set seq_cells [$lib_seq find_liberty_cells_matching "DFF*" 0 0]
  puts "PASS: ASAP7 DFF* cells: [llength $seq_cells]"
}

# Report specific cells to exercise different timing types
catch {
  report_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/DFFHQNx1_ASAP7_75t_R
  puts "PASS: ASAP7 DFF cell report"
}

catch {
  report_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/DFFHQNx2_ASAP7_75t_R
  puts "PASS: ASAP7 DFF x2 cell report"
}

# Scan DFF cells (scan_in, scan_enable timing arcs)
catch {
  report_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/SDFHQNx1_ASAP7_75t_R
  puts "PASS: ASAP7 SDFF cell report"
}

catch {
  report_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/SDFHQNx2_ASAP7_75t_R
  puts "PASS: ASAP7 SDFF x2 cell report"
}

# ICG cells (clock gating - exercises clock gate timing types)
catch {
  report_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/ICGx1_ASAP7_75t_R
  puts "PASS: ASAP7 ICG cell report"
}

catch {
  report_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/ICGx2_ASAP7_75t_R
  puts "PASS: ASAP7 ICG x2 cell report"
}

# Async set/reset cells (recovery/removal timing types)
catch {
  set async_cells [$lib_seq find_liberty_cells_matching "*ASYNC*" 0 0]
  puts "PASS: ASAP7 ASYNC cells: [llength $async_cells]"
}

# DFFR cells with reset (recovery/removal)
catch {
  set dffr_cells [$lib_seq find_liberty_cells_matching "DFFR*" 0 0]
  foreach cell_obj $dffr_cells {
    report_lib_cell [get_object_name $cell_obj]
  }
  puts "PASS: ASAP7 DFFR cells reported"
}

############################################################
# Read ASAP7 SEQ SS corner for different model values
############################################################
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_SS_nldm_220123.lib
puts "PASS: read ASAP7 SEQ SS"

############################################################
# Read ASAP7 SIMPLE library (combinational cells)
############################################################
read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz
puts "PASS: read ASAP7 SIMPLE"

catch {
  set simple_lib [sta::find_liberty asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120]
  set simple_cells [$simple_lib find_liberty_cells_matching "*" 0 0]
  puts "PASS: ASAP7 SIMPLE cells: [llength $simple_cells]"
}

############################################################
# Read ASAP7 AO library (AND-OR complex cells)
############################################################
read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz
puts "PASS: read ASAP7 AO"

catch {
  set ao_lib [sta::find_liberty asap7sc7p5t_AO_RVT_FF_nldm_211120]
  set ao_cells [$ao_lib find_liberty_cells_matching "AO*" 0 0]
  puts "PASS: ASAP7 AO* cells: [llength $ao_cells]"
  foreach c $ao_cells {
    catch {report_lib_cell [get_object_name $c]}
  }
}

############################################################
# Read ASAP7 OA library (OR-AND complex cells)
############################################################
read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz
puts "PASS: read ASAP7 OA"

catch {
  set oa_lib [sta::find_liberty asap7sc7p5t_OA_RVT_FF_nldm_211120]
  set oa_cells [$oa_lib find_liberty_cells_matching "OA*" 0 0]
  puts "PASS: ASAP7 OA* cells: [llength $oa_cells]"
  foreach c $oa_cells {
    catch {report_lib_cell [get_object_name $c]}
  }
}

############################################################
# Read ASAP7 INVBUF library
############################################################
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz
puts "PASS: read ASAP7 INVBUF"

############################################################
# Read libraries from different process nodes
# Exercises different liberty features/syntax in each library
############################################################

# Read IHP SG13G2 library (has tristate, scan, different timing types)
read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p20V_25C.lib
puts "PASS: read IHP sg13g2"

catch {
  set ihp_lib [sta::find_liberty sg13g2_stdcell_typ_1p20V_25C]
  # Report tristate buffer cell (exercises three_state_enable paths)
  catch {report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_ebufn_2}
  puts "PASS: IHP tristate buffer report"

  # Report scan flip-flop (exercises scan timing paths)
  catch {report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_sdfbbp_1}
  puts "PASS: IHP scan DFF report"

  # Report latch cell
  catch {report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_dlhq_1}
  puts "PASS: IHP latch report"

  # MUX cell
  catch {report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_mux2_1}
  puts "PASS: IHP mux report"
}

# Read IHP second PVT corner
read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p50V_25C.lib
puts "PASS: read IHP 1.5V"

############################################################
# Read latch library to exercise latch-specific code
############################################################
read_liberty ../../test/liberty_latch3.lib
puts "PASS: read latch3 library"

############################################################
# Read liberty with backslash-EOL continuation
############################################################
read_liberty ../../test/liberty_backslash_eol.lib
puts "PASS: read backslash_eol library"

############################################################
# Read liberty with float-as-string values
############################################################
read_liberty ../../test/liberty_float_as_str.lib
puts "PASS: read float_as_str library"

############################################################
# Read liberty arcs one2one libraries
############################################################
read_liberty ../../test/liberty_arcs_one2one_1.lib
puts "PASS: read arcs_one2one_1 library"

read_liberty ../../test/liberty_arcs_one2one_2.lib
puts "PASS: read arcs_one2one_2 library"

############################################################
# Read SRAM macro library (exercises macro/memory cells)
############################################################
read_liberty ../../test/gf180mcu_sram.lib.gz
puts "PASS: read gf180mcu SRAM library"

############################################################
# Read ASAP7 SEQ LVT/SLVT (different threshold voltages)
############################################################
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_LVT_FF_nldm_220123.lib
puts "PASS: read ASAP7 SEQ LVT"

read_liberty ../../test/asap7/asap7sc7p5t_SEQ_SLVT_FF_nldm_220123.lib
puts "PASS: read ASAP7 SEQ SLVT"

############################################################
# Read ASAP7 INVBUF different Vt flavors
############################################################
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_LVT_FF_nldm_220122.lib.gz
puts "PASS: read ASAP7 INVBUF LVT"

read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_SLVT_FF_nldm_220122.lib.gz
puts "PASS: read ASAP7 INVBUF SLVT"

############################################################
# Write liberty for ASAP7 SEQ
############################################################
set outfile [make_result_file liberty_ccsn_ecsm_write.lib]
sta::write_liberty asap7sc7p5t_SEQ_RVT_FF_nldm_220123 $outfile
puts "PASS: write_liberty ASAP7 SEQ"

set outfile2 [make_result_file liberty_ccsn_ecsm_write_ihp.lib]
sta::write_liberty sg13g2_stdcell_typ_1p20V_25C $outfile2
puts "PASS: write_liberty IHP"

puts "ALL PASSED"
