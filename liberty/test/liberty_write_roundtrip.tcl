# Test liberty write and verify output for code coverage
# Targets: LibertyWriter.cc (all write functions: writeHeader, writeCells, writePort,
#          writePwrGndPort, writeBusPort, writeTimingArcSet, writeTimingModels,
#          writeTableModel0/1/2, writeTableTemplates, writeBusDcls, timingTypeString),
#          TableModel.cc (table iteration during write),
#          TimingArc.cc (arc iteration during write),
#          Liberty.cc (property queries during write)
source ../../test/helpers.tcl

############################################################
# Read and write Nangate45 (comprehensive cell library)
############################################################

read_liberty ../../test/nangate45/Nangate45_typ.lib
puts "PASS: read Nangate45 original"

# Write liberty - this exercises most of LibertyWriter.cc
set outfile1 [make_result_file liberty_roundtrip_nangate.lib]
sta::write_liberty NangateOpenCellLibrary $outfile1
puts "PASS: write Nangate45"

# Verify output file exists and has content
set fsize [file size $outfile1]
if { $fsize > 1000 } {
  puts "PASS: Nangate45 output file has $fsize bytes"
} else {
  puts "FAIL: Nangate45 output file too small"
}

############################################################
# Read and write Sky130 (large library with different features)
############################################################

read_liberty ../../test/sky130hd/sky130hd_tt.lib
puts "PASS: read Sky130"

set outfile2 [make_result_file liberty_roundtrip_sky130.lib]
sta::write_liberty sky130_fd_sc_hd__tt_025C_1v80 $outfile2
puts "PASS: write Sky130"

set fsize [file size $outfile2]
if { $fsize > 1000 } {
  puts "PASS: Sky130 output file has $fsize bytes"
}

############################################################
# Read and write IHP (different vendor format)
############################################################

read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p20V_25C.lib
puts "PASS: read IHP"

set outfile3 [make_result_file liberty_roundtrip_ihp.lib]
sta::write_liberty sg13g2_stdcell_typ_1p20V_25C $outfile3
puts "PASS: write IHP"

set fsize [file size $outfile3]
if { $fsize > 1000 } {
  puts "PASS: IHP output file has $fsize bytes"
}

############################################################
# Read and write ASAP7 SIMPLE (compressed input)
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz
puts "PASS: read ASAP7 SIMPLE compressed"

set outfile4 [make_result_file liberty_roundtrip_asap7_simple.lib]
sta::write_liberty asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120 $outfile4
puts "PASS: write ASAP7 SIMPLE"

set fsize [file size $outfile4]
if { $fsize > 1000 } {
  puts "PASS: ASAP7 SIMPLE output file has $fsize bytes"
}

############################################################
# Read and write ASAP7 SEQ (sequential cell writing)
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
puts "PASS: read ASAP7 SEQ"

set outfile5 [make_result_file liberty_roundtrip_asap7_seq.lib]
sta::write_liberty asap7sc7p5t_SEQ_RVT_FF_nldm_220123 $outfile5
puts "PASS: write ASAP7 SEQ"

set fsize [file size $outfile5]
if { $fsize > 1000 } {
  puts "PASS: ASAP7 SEQ output file has $fsize bytes"
}

############################################################
# Read and write ASAP7 INVBUF (compressed input)
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz
puts "PASS: read ASAP7 INVBUF compressed"

set outfile6 [make_result_file liberty_roundtrip_asap7_invbuf.lib]
sta::write_liberty asap7sc7p5t_INVBUF_RVT_FF_nldm_211120 $outfile6
puts "PASS: write ASAP7 INVBUF"

set fsize [file size $outfile6]
if { $fsize > 1000 } {
  puts "PASS: ASAP7 INVBUF output file has $fsize bytes"
}

############################################################
# Read and write ASAP7 AO (AND-OR cells)
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz
puts "PASS: read ASAP7 AO"

set outfile7 [make_result_file liberty_roundtrip_asap7_ao.lib]
sta::write_liberty asap7sc7p5t_AO_RVT_FF_nldm_211120 $outfile7
puts "PASS: write ASAP7 AO"

############################################################
# Read and write ASAP7 OA (OR-AND cells)
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz
puts "PASS: read ASAP7 OA"

set outfile8 [make_result_file liberty_roundtrip_asap7_oa.lib]
sta::write_liberty asap7sc7p5t_OA_RVT_FF_nldm_211120 $outfile8
puts "PASS: write ASAP7 OA"

############################################################
# Read and write fakeram (SRAM macro with bus ports)
############################################################

read_liberty ../../test/asap7/fakeram7_256x32.lib
puts "PASS: read fakeram7_256x32"

set outfile9 [make_result_file liberty_roundtrip_fakeram.lib]
sta::write_liberty fakeram7_256x32 $outfile9
puts "PASS: write fakeram7_256x32"

set fsize [file size $outfile9]
if { $fsize > 100 } {
  puts "PASS: fakeram output file has $fsize bytes"
}

############################################################
# Read and write fake_macros
############################################################

read_liberty ../../test/nangate45/fake_macros.lib
puts "PASS: read fake_macros"

set outfile10 [make_result_file liberty_roundtrip_fake_macros.lib]
sta::write_liberty fake_macros $outfile10
puts "PASS: write fake_macros"

############################################################
# Read and write Nangate45 fast (different corner parameters)
############################################################

read_liberty ../../test/nangate45/Nangate45_fast.lib
puts "PASS: read Nangate45 fast"

set outfile11 [make_result_file liberty_roundtrip_nangate_fast.lib]
sta::write_liberty NangateOpenCellLibrary_fast $outfile11
puts "PASS: write Nangate45 fast"

############################################################
# Read and write Nangate45 slow
############################################################

read_liberty ../../test/nangate45/Nangate45_slow.lib
puts "PASS: read Nangate45 slow"

set outfile12 [make_result_file liberty_roundtrip_nangate_slow.lib]
sta::write_liberty NangateOpenCellLibrary_slow $outfile12
puts "PASS: write Nangate45 slow"

############################################################
# Read and write Nangate45 LVT
############################################################

read_liberty ../../test/nangate45/Nangate45_lvt.lib
puts "PASS: read Nangate45 LVT"

set outfile13 [make_result_file liberty_roundtrip_nangate_lvt.lib]
sta::write_liberty NangateOpenCellLibrary_lvt $outfile13
puts "PASS: write Nangate45 LVT"

############################################################
# Read and write multiple fakeram sizes
############################################################

read_liberty ../../test/nangate45/fakeram45_256x16.lib
puts "PASS: read fakeram45_256x16"

set outfile14 [make_result_file liberty_roundtrip_fakeram45_256x16.lib]
sta::write_liberty fakeram45_256x16 $outfile14
puts "PASS: write fakeram45_256x16"

read_liberty ../../test/nangate45/fakeram45_64x32.lib
puts "PASS: read fakeram45_64x32"

set outfile15 [make_result_file liberty_roundtrip_fakeram45_64x32.lib]
sta::write_liberty fakeram45_64x32 $outfile15
puts "PASS: write fakeram45_64x32"

############################################################
# Read and write ASAP7 SS corner (different operating conditions)
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_SS_nldm_211120.lib.gz
puts "PASS: read ASAP7 SIMPLE SS"

set outfile17 [make_result_file liberty_roundtrip_asap7_ss.lib]
sta::write_liberty asap7sc7p5t_SIMPLE_RVT_SS_nldm_211120 $outfile17
puts "PASS: write ASAP7 SIMPLE SS"

############################################################
# Read and write Sky130 FF corner
############################################################

read_liberty ../../test/sky130hd/sky130_fd_sc_hd__ff_n40C_1v95.lib
puts "PASS: read Sky130 FF"

set outfile18 [make_result_file liberty_roundtrip_sky130_ff.lib]
sta::write_liberty sky130_fd_sc_hd__ff_n40C_1v95 $outfile18
puts "PASS: write Sky130 FF"

############################################################
# Read and write Sky130 SS corner
############################################################

read_liberty ../../test/sky130hd/sky130_fd_sc_hd__ss_n40C_1v40.lib
puts "PASS: read Sky130 SS"

set outfile19 [make_result_file liberty_roundtrip_sky130_ss.lib]
sta::write_liberty sky130_fd_sc_hd__ss_n40C_1v40 $outfile19
puts "PASS: write Sky130 SS"

puts "ALL PASSED"
