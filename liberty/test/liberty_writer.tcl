# Test liberty writer and advanced reading features for code coverage
# Targets: LibertyWriter.cc, LibertyReader.cc (more paths), LibertyBuilder.cc
source ../../test/helpers.tcl

############################################################
# Read and write various libraries
############################################################

# Read Nangate45
read_liberty ../../test/nangate45/Nangate45_typ.lib
puts "PASS: read Nangate45"

set outfile1 [make_result_file liberty_write_nangate.lib]
sta::write_liberty NangateOpenCellLibrary $outfile1
puts "PASS: write_liberty Nangate45"

# Read ASAP7 SEQ (exercises different cell types)
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
puts "PASS: read ASAP7 SEQ"

set outfile2 [make_result_file liberty_write_asap7_seq.lib]
sta::write_liberty asap7sc7p5t_SEQ_RVT_FF_nldm_220123 $outfile2
puts "PASS: write_liberty ASAP7 SEQ"

# Read ASAP7 SIMPLE (combinational cells)
read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz
puts "PASS: read ASAP7 SIMPLE"

set outfile3 [make_result_file liberty_write_asap7_simple.lib]
sta::write_liberty asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120 $outfile3
puts "PASS: write_liberty ASAP7 SIMPLE"

# Read IHP library
read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p20V_25C.lib
puts "PASS: read IHP"

set outfile4 [make_result_file liberty_write_ihp.lib]
sta::write_liberty sg13g2_stdcell_typ_1p20V_25C $outfile4
puts "PASS: write_liberty IHP"

# Read Sky130 library
read_liberty ../../test/sky130hd/sky130hd_tt.lib
puts "PASS: read Sky130"

# Read CCSN library (compressed, exercises CCSN-specific paths)
read_liberty ../../test/asap7_ccsn.lib.gz
puts "PASS: read CCSN library"

# Read latch library
read_liberty ../../test/liberty_latch3.lib
puts "PASS: read latch library"

# Read SRAM macro library
read_liberty ../../test/asap7/fakeram7_256x32.lib
puts "PASS: read fakeram ASAP7"

read_liberty ../../test/nangate45/fakeram45_256x16.lib
puts "PASS: read fakeram Nangate45"

read_liberty ../../test/nangate45/fake_macros.lib
puts "PASS: read fake_macros"

# Read liberty_float_as_str
read_liberty ../../test/liberty_float_as_str.lib
puts "PASS: read liberty_float_as_str"

# Read liberty_backslash_eol
read_liberty ../../test/liberty_backslash_eol.lib
puts "PASS: read liberty_backslash_eol"

# Read liberty_arcs_one2one
read_liberty ../../test/liberty_arcs_one2one_1.lib
puts "PASS: read liberty_arcs_one2one_1"

read_liberty ../../test/liberty_arcs_one2one_2.lib
puts "PASS: read liberty_arcs_one2one_2"

############################################################
# Additional ASAP7 variants (different Vt/corner combos)
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_SEQ_LVT_FF_nldm_220123.lib
puts "PASS: read ASAP7 SEQ LVT FF"

read_liberty ../../test/asap7/asap7sc7p5t_SEQ_SLVT_FF_nldm_220123.lib
puts "PASS: read ASAP7 SEQ SLVT FF"

read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_LVT_FF_nldm_220122.lib.gz
puts "PASS: read ASAP7 INVBUF LVT FF"

read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_SLVT_FF_nldm_220122.lib.gz
puts "PASS: read ASAP7 INVBUF SLVT FF"

read_liberty ../../test/asap7/asap7sc7p5t_AO_LVT_FF_nldm_211120.lib.gz
puts "PASS: read ASAP7 AO LVT FF"

read_liberty ../../test/asap7/asap7sc7p5t_AO_SLVT_FF_nldm_211120.lib.gz
puts "PASS: read ASAP7 AO SLVT FF"

read_liberty ../../test/asap7/asap7sc7p5t_OA_LVT_FF_nldm_211120.lib.gz
puts "PASS: read ASAP7 OA LVT FF"

read_liberty ../../test/asap7/asap7sc7p5t_OA_SLVT_FF_nldm_211120.lib.gz
puts "PASS: read ASAP7 OA SLVT FF"

read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_LVT_FF_nldm_211120.lib.gz
puts "PASS: read ASAP7 SIMPLE LVT FF"

read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_SLVT_FF_nldm_211120.lib.gz
puts "PASS: read ASAP7 SIMPLE SLVT FF"

############################################################
# TT corners
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_TT_nldm_220122.lib.gz
puts "PASS: read ASAP7 INVBUF RVT TT"

read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_LVT_TT_nldm_220122.lib.gz
puts "PASS: read ASAP7 INVBUF LVT TT"

read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_SLVT_TT_nldm_220122.lib.gz
puts "PASS: read ASAP7 INVBUF SLVT TT"

############################################################
# SS corners
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_SS_nldm_211120.lib.gz
puts "PASS: read ASAP7 AO RVT SS"

read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_SS_nldm_211120.lib.gz
puts "PASS: read ASAP7 OA RVT SS"

############################################################
# Sky130 variants
############################################################

read_liberty ../../test/sky130hd/sky130_fd_sc_hd__ff_n40C_1v95.lib
puts "PASS: read Sky130 FF"

read_liberty ../../test/sky130hd/sky130_fd_sc_hd__ss_n40C_1v40.lib
puts "PASS: read Sky130 SS"

read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
puts "PASS: read Sky130 TT"

# Read sky130hs variants
read_liberty ../../test/sky130hs/sky130_fd_sc_hs__tt_025C_1v80.lib
puts "PASS: read Sky130HS TT"

# GF180MCU SRAM
read_liberty ../../test/gf180mcu_sram.lib.gz
puts "PASS: read gf180mcu_sram"

puts "ALL PASSED"
