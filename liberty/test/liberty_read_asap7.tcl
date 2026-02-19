# Test ASAP7 library reading for code coverage
# Tests multiple lib reading including compressed (.lib.gz) files

############################################################
# Read uncompressed ASAP7 SEQ library
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib

set seq_lib [get_libs asap7sc7p5t_SEQ_RVT_FF_nldm_220123]
if { $seq_lib == "" } {
  puts "FAIL: SEQ RVT FF library not found"
  exit 1
}

# Query DFF cells
set dff [get_lib_cells asap7sc7p5t_SEQ_RVT_FF_nldm_220123/DFFHQNx1_ASAP7_75t_R]
if { $dff == "" } {
  puts "FAIL: DFFHQNx1 not found"
  exit 1
}

report_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/DFFHQNx1_ASAP7_75t_R

# Query latch cells
set latch [get_lib_cells asap7sc7p5t_SEQ_RVT_FF_nldm_220123/DLLx1_ASAP7_75t_R]
if { $latch == "" } {
  puts "FAIL: DLLx1 not found"
  exit 1
}

report_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/DLLx1_ASAP7_75t_R

# Query ICG (integrated clock gate) cells
set icg [get_lib_cells asap7sc7p5t_SEQ_RVT_FF_nldm_220123/ICGx1_ASAP7_75t_R]
if { $icg == "" } {
  puts "FAIL: ICGx1 not found"
  exit 1
}

report_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/ICGx1_ASAP7_75t_R

# Query all cells in SEQ library
set all_seq_cells [get_lib_cells asap7sc7p5t_SEQ_RVT_FF_nldm_220123/*]

# Get pins of DFF
set dff_pins [get_lib_pins asap7sc7p5t_SEQ_RVT_FF_nldm_220123/DFFHQNx1_ASAP7_75t_R/*]

############################################################
# Read compressed ASAP7 SIMPLE library (.lib.gz)
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz

set simple_lib [get_libs asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120]
if { $simple_lib == "" } {
  puts "FAIL: SIMPLE RVT FF library not found"
  exit 1
}

# Query cells in SIMPLE library
set all_simple_cells [get_lib_cells asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120/*]

############################################################
# Read compressed ASAP7 INVBUF library (.lib.gz)
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz

set invbuf_lib [get_libs asap7sc7p5t_INVBUF_RVT_FF_nldm_211120]
if { $invbuf_lib == "" } {
  puts "FAIL: INVBUF RVT FF library not found"
  exit 1
}

set all_invbuf_cells [get_lib_cells asap7sc7p5t_INVBUF_RVT_FF_nldm_211120/*]

############################################################
# Read compressed ASAP7 OA library (.lib.gz)
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz

set oa_lib [get_libs asap7sc7p5t_OA_RVT_FF_nldm_211120]
if { $oa_lib == "" } {
  puts "FAIL: OA RVT FF library not found"
  exit 1
}

set all_oa_cells [get_lib_cells asap7sc7p5t_OA_RVT_FF_nldm_211120/*]

############################################################
# Read compressed ASAP7 AO library (.lib.gz)
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz

set ao_lib [get_libs asap7sc7p5t_AO_RVT_FF_nldm_211120]
if { $ao_lib == "" } {
  puts "FAIL: AO RVT FF library not found"
  exit 1
}

set all_ao_cells [get_lib_cells asap7sc7p5t_AO_RVT_FF_nldm_211120/*]

############################################################
# Read SS corner for different timing
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_SS_nldm_220123.lib

read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_SS_nldm_211120.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_SS_nldm_220122.lib.gz

############################################################
# Read CCSN library (compressed, exercises LibertyReader CCSN)
############################################################

read_liberty ../../test/asap7_ccsn.lib.gz

############################################################
# Read latch library (exercises latch-specific parsing)
############################################################

read_liberty ../../test/liberty_latch3.lib

############################################################
# Read fakeram (macro library)
############################################################

read_liberty ../../test/nangate45/fake_macros.lib

read_liberty ../../test/nangate45/fakeram45_256x16.lib
