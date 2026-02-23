# Test liberty write and verify output for code coverage
# Targets: LibertyWriter.cc (all write functions: writeHeader, writeCells, writePort,
#          writePwrGndPort, writeBusPort, writeTimingArcSet, writeTimingModels,
#          writeTableModel0/1/2, writeTableTemplates, writeBusDcls, timingTypeString),
#          TableModel.cc (table iteration during write),
#          TimingArc.cc (arc iteration during write),
#          Liberty.cc (property queries during write)
source ../../test/helpers.tcl

proc assert_written_liberty {path lib_name} {
  if {![file exists $path]} {
    error "missing written liberty file: $path"
  }
  set in [open $path r]
  set text [read $in]
  close $in
  if {[string length $text] <= 0} {
    error "written liberty file is empty: $path"
  }

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
# Read and write Nangate45 (comprehensive cell library)
############################################################

read_liberty ../../test/nangate45/Nangate45_typ.lib

# Write liberty - this exercises most of LibertyWriter.cc
set outfile1 [make_result_file liberty_roundtrip_nangate.lib]
sta::write_liberty NangateOpenCellLibrary $outfile1
assert_written_liberty $outfile1 NangateOpenCellLibrary

diff_files liberty_roundtrip_nangate.libok $outfile1

############################################################
# Read and write Sky130 (large library with different features)
############################################################

read_liberty ../../test/sky130hd/sky130hd_tt.lib

set outfile2 [make_result_file liberty_roundtrip_sky130.lib]
sta::write_liberty sky130_fd_sc_hd__tt_025C_1v80 $outfile2
assert_written_liberty $outfile2 sky130_fd_sc_hd__tt_025C_1v80

diff_files liberty_roundtrip_sky130.libok $outfile2

############################################################
# Read and write IHP (different vendor format)
############################################################

read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p20V_25C.lib

set outfile3 [make_result_file liberty_roundtrip_ihp.lib]
sta::write_liberty sg13g2_stdcell_typ_1p20V_25C $outfile3
assert_written_liberty $outfile3 sg13g2_stdcell_typ_1p20V_25C

diff_files liberty_roundtrip_ihp.libok $outfile3

############################################################
# Read and write ASAP7 SIMPLE (compressed input)
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz

set outfile4 [make_result_file liberty_roundtrip_asap7_simple.lib]
sta::write_liberty asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120 $outfile4
assert_written_liberty $outfile4 asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120

diff_files liberty_roundtrip_asap7_simple.libok $outfile4

############################################################
# Read and write ASAP7 SEQ (sequential cell writing)
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib

set outfile5 [make_result_file liberty_roundtrip_asap7_seq.lib]
sta::write_liberty asap7sc7p5t_SEQ_RVT_FF_nldm_220123 $outfile5
assert_written_liberty $outfile5 asap7sc7p5t_SEQ_RVT_FF_nldm_220123

diff_files liberty_roundtrip_asap7_seq.libok $outfile5

############################################################
# Read and write ASAP7 INVBUF (compressed input)
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz

set outfile6 [make_result_file liberty_roundtrip_asap7_invbuf.lib]
sta::write_liberty asap7sc7p5t_INVBUF_RVT_FF_nldm_211120 $outfile6
assert_written_liberty $outfile6 asap7sc7p5t_INVBUF_RVT_FF_nldm_211120

diff_files liberty_roundtrip_asap7_invbuf.libok $outfile6

############################################################
# Read and write ASAP7 AO (AND-OR cells)
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz

set outfile7 [make_result_file liberty_roundtrip_asap7_ao.lib]
sta::write_liberty asap7sc7p5t_AO_RVT_FF_nldm_211120 $outfile7
assert_written_liberty $outfile7 asap7sc7p5t_AO_RVT_FF_nldm_211120

############################################################
# Read and write ASAP7 OA (OR-AND cells)
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz

set outfile8 [make_result_file liberty_roundtrip_asap7_oa.lib]
sta::write_liberty asap7sc7p5t_OA_RVT_FF_nldm_211120 $outfile8
assert_written_liberty $outfile8 asap7sc7p5t_OA_RVT_FF_nldm_211120

############################################################
# Read and write fakeram (SRAM macro with bus ports)
############################################################

read_liberty ../../test/asap7/fakeram7_256x32.lib

set outfile9 [make_result_file liberty_roundtrip_fakeram.lib]
sta::write_liberty fakeram7_256x32 $outfile9
assert_written_liberty $outfile9 fakeram7_256x32

diff_files liberty_roundtrip_fakeram.libok $outfile9

############################################################
# Read and write fake_macros
############################################################

read_liberty ../../test/nangate45/fake_macros.lib

set outfile10 [make_result_file liberty_roundtrip_fake_macros.lib]
sta::write_liberty fake_macros $outfile10
assert_written_liberty $outfile10 fake_macros

############################################################
# Read and write Nangate45 fast (different corner parameters)
############################################################

read_liberty ../../test/nangate45/Nangate45_fast.lib

set outfile11 [make_result_file liberty_roundtrip_nangate_fast.lib]
sta::write_liberty NangateOpenCellLibrary_fast $outfile11
assert_written_liberty $outfile11 NangateOpenCellLibrary_fast

############################################################
# Read and write Nangate45 slow
############################################################

read_liberty ../../test/nangate45/Nangate45_slow.lib

set outfile12 [make_result_file liberty_roundtrip_nangate_slow.lib]
sta::write_liberty NangateOpenCellLibrary_slow $outfile12
assert_written_liberty $outfile12 NangateOpenCellLibrary_slow

############################################################
# Read and write Nangate45 LVT
############################################################

read_liberty ../../test/nangate45/Nangate45_lvt.lib

set outfile13 [make_result_file liberty_roundtrip_nangate_lvt.lib]
sta::write_liberty NangateOpenCellLibrary_lvt $outfile13
assert_written_liberty $outfile13 NangateOpenCellLibrary_lvt

############################################################
# Read and write multiple fakeram sizes
############################################################

read_liberty ../../test/nangate45/fakeram45_256x16.lib

set outfile14 [make_result_file liberty_roundtrip_fakeram45_256x16.lib]
sta::write_liberty fakeram45_256x16 $outfile14
assert_written_liberty $outfile14 fakeram45_256x16

read_liberty ../../test/nangate45/fakeram45_64x32.lib

set outfile15 [make_result_file liberty_roundtrip_fakeram45_64x32.lib]
sta::write_liberty fakeram45_64x32 $outfile15
assert_written_liberty $outfile15 fakeram45_64x32

############################################################
# Read and write ASAP7 SS corner (different operating conditions)
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_SS_nldm_211120.lib.gz

set outfile17 [make_result_file liberty_roundtrip_asap7_ss.lib]
sta::write_liberty asap7sc7p5t_SIMPLE_RVT_SS_nldm_211120 $outfile17
assert_written_liberty $outfile17 asap7sc7p5t_SIMPLE_RVT_SS_nldm_211120

############################################################
# Read and write Sky130 FF corner
############################################################

read_liberty ../../test/sky130hd/sky130_fd_sc_hd__ff_n40C_1v95.lib

set outfile18 [make_result_file liberty_roundtrip_sky130_ff.lib]
sta::write_liberty sky130_fd_sc_hd__ff_n40C_1v95 $outfile18
assert_written_liberty $outfile18 sky130_fd_sc_hd__ff_n40C_1v95

############################################################
# Read and write Sky130 SS corner
############################################################

read_liberty ../../test/sky130hd/sky130_fd_sc_hd__ss_n40C_1v40.lib

set outfile19 [make_result_file liberty_roundtrip_sky130_ss.lib]
sta::write_liberty sky130_fd_sc_hd__ss_n40C_1v40 $outfile19
assert_written_liberty $outfile19 sky130_fd_sc_hd__ss_n40C_1v40
