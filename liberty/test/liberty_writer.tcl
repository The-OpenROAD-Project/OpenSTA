# Test liberty writer and advanced reading features for code coverage
# Targets: LibertyWriter.cc, LibertyReader.cc (more paths), LibertyBuilder.cc
source ../../test/helpers.tcl
suppress_msg 1140

proc assert_written_liberty {path lib_name} {
  if {![file exists $path]} {
    error "missing written liberty file: $path"
  }
  if {[file size $path] <= 0} {
    error "written liberty file is empty: $path"
  }

  set in [open $path r]
  set text [read $in]
  close $in

  if {[string first "library (" $text] < 0} {
    error "written liberty file has no library block: $path"
  }
  if {[string first $lib_name $text] < 0} {
    error "written liberty file does not contain library name '$lib_name': $path"
  }
  if {![regexp {cell[[:space:]]*\(} $text]} {
    error "written liberty file has no cell blocks: $path"
  }
}

############################################################
# Read and write various libraries
############################################################

# Read Nangate45
read_liberty ../../test/nangate45/Nangate45_typ.lib

set outfile1 [make_result_file liberty_write_nangate.lib]
sta::write_liberty NangateOpenCellLibrary $outfile1
assert_written_liberty $outfile1 NangateOpenCellLibrary

# Read ASAP7 SEQ (exercises different cell types)
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib

set outfile2 [make_result_file liberty_write_asap7_seq.lib]
sta::write_liberty asap7sc7p5t_SEQ_RVT_FF_nldm_220123 $outfile2
assert_written_liberty $outfile2 asap7sc7p5t_SEQ_RVT_FF_nldm_220123

# Read ASAP7 SIMPLE (combinational cells)
read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz

set outfile3 [make_result_file liberty_write_asap7_simple.lib]
sta::write_liberty asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120 $outfile3
assert_written_liberty $outfile3 asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120

# Read IHP library
read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p20V_25C.lib

set outfile4 [make_result_file liberty_write_ihp.lib]
sta::write_liberty sg13g2_stdcell_typ_1p20V_25C $outfile4
assert_written_liberty $outfile4 sg13g2_stdcell_typ_1p20V_25C

# Read Sky130 library
read_liberty ../../test/sky130hd/sky130hd_tt.lib

# Read CCSN library (compressed, exercises CCSN-specific paths)
read_liberty ../../test/asap7_ccsn.lib.gz

# Read latch library
read_liberty ../../test/liberty_latch3.lib

# Read SRAM macro library
read_liberty ../../test/asap7/fakeram7_256x32.lib

read_liberty ../../test/nangate45/fakeram45_256x16.lib

read_liberty ../../test/nangate45/fake_macros.lib

# Read liberty_float_as_str
read_liberty ../../test/liberty_float_as_str.lib

# Read liberty_backslash_eol
read_liberty ../../test/liberty_backslash_eol.lib

# Read liberty_arcs_one2one
read_liberty ../../test/liberty_arcs_one2one_1.lib

read_liberty ../../test/liberty_arcs_one2one_2.lib

############################################################
# Additional ASAP7 variants (different Vt/corner combos)
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_SEQ_LVT_FF_nldm_220123.lib

read_liberty ../../test/asap7/asap7sc7p5t_SEQ_SLVT_FF_nldm_220123.lib

read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_LVT_FF_nldm_220122.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_SLVT_FF_nldm_220122.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_AO_LVT_FF_nldm_211120.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_AO_SLVT_FF_nldm_211120.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_OA_LVT_FF_nldm_211120.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_OA_SLVT_FF_nldm_211120.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_LVT_FF_nldm_211120.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_SLVT_FF_nldm_211120.lib.gz

############################################################
# TT corners
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_TT_nldm_220122.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_LVT_TT_nldm_220122.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_SLVT_TT_nldm_220122.lib.gz

############################################################
# SS corners
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_SS_nldm_211120.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_SS_nldm_211120.lib.gz

############################################################
# Sky130 variants
############################################################

read_liberty ../../test/sky130hd/sky130_fd_sc_hd__ff_n40C_1v95.lib

read_liberty ../../test/sky130hd/sky130_fd_sc_hd__ss_n40C_1v40.lib

read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib

# Read sky130hs variants
read_liberty ../../test/sky130hs/sky130_fd_sc_hs__tt_025C_1v80.lib

# GF180MCU SRAM
read_liberty ../../test/gf180mcu_sram.lib.gz

if {[llength [get_libs *]] < 20} {
  error "expected to load many liberty libraries, but got [llength [get_libs *]]"
}
