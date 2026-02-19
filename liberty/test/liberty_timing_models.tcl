# Test timing arc/model queries and various cell types for code coverage
# Targets: TimingArc.cc, TableModel.cc, Liberty.cc (deep model queries),
#          LibertyReader.cc (timing type parsing), Sequential.cc
source ../../test/helpers.tcl

############################################################
# Read libraries with different timing model types
############################################################

read_liberty ../../test/nangate45/Nangate45_typ.lib

read_verilog ../../sdc/test/sdc_test2.v
link_design sdc_test2

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_input_delay -clock clk1 2.0 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]

############################################################
# Query timing arcs on various cell types
############################################################

# Combinational cells - report_lib_cell shows timing arcs
report_lib_cell NangateOpenCellLibrary/INV_X1

report_lib_cell NangateOpenCellLibrary/BUF_X1

report_lib_cell NangateOpenCellLibrary/NAND2_X1

report_lib_cell NangateOpenCellLibrary/NOR2_X1

report_lib_cell NangateOpenCellLibrary/AOI21_X1

report_lib_cell NangateOpenCellLibrary/OAI21_X1

report_lib_cell NangateOpenCellLibrary/AND2_X1

report_lib_cell NangateOpenCellLibrary/OR2_X1

# XOR cells
report_lib_cell NangateOpenCellLibrary/XOR2_X1

report_lib_cell NangateOpenCellLibrary/XNOR2_X1

# Full/Half adder
report_lib_cell NangateOpenCellLibrary/FA_X1

report_lib_cell NangateOpenCellLibrary/HA_X1

# MUX
report_lib_cell NangateOpenCellLibrary/MUX2_X1

# Tristate
report_lib_cell NangateOpenCellLibrary/TINV_X1

############################################################
# Sequential cells (timing arcs: setup, hold, clk-to-q)
############################################################

report_lib_cell NangateOpenCellLibrary/DFF_X1

report_lib_cell NangateOpenCellLibrary/DFF_X2

report_lib_cell NangateOpenCellLibrary/DFFR_X1

report_lib_cell NangateOpenCellLibrary/DFFS_X1

report_lib_cell NangateOpenCellLibrary/DFFRS_X1

# Scan DFFs
report_lib_cell NangateOpenCellLibrary/SDFF_X1

report_lib_cell NangateOpenCellLibrary/SDFFR_X1

report_lib_cell NangateOpenCellLibrary/SDFFS_X1

report_lib_cell NangateOpenCellLibrary/SDFFRS_X1

# Latch
report_lib_cell NangateOpenCellLibrary/TLAT_X1

# Clock gate
report_lib_cell NangateOpenCellLibrary/CLKGATETST_X1

report_lib_cell NangateOpenCellLibrary/CLKGATETST_X2

############################################################
# Query timing paths (exercises timing arc evaluation)
############################################################

report_checks -from [get_ports in1] -to [get_ports out1]

report_checks -from [get_ports in2] -to [get_ports out1]

report_checks -from [get_ports in1] -to [get_ports out2]

# Min delay paths
report_checks -path_delay min -from [get_ports in1] -to [get_ports out1]

# Rise/fall reports
report_checks -from [get_ports in1] -rise_to [get_ports out1]

report_checks -from [get_ports in1] -fall_to [get_ports out1]

############################################################
# Drive strength variations (larger cells with different tables)
############################################################

foreach size {1 2 4 8 16 32} {
  report_lib_cell NangateOpenCellLibrary/INV_X${size}
}

foreach size {1 2 4 8 16 32} {
  report_lib_cell NangateOpenCellLibrary/BUF_X${size}
}

foreach size {1 2 4} {
  report_lib_cell NangateOpenCellLibrary/NAND2_X${size}
  report_lib_cell NangateOpenCellLibrary/NAND3_X${size}
  report_lib_cell NangateOpenCellLibrary/NAND4_X${size}
  report_lib_cell NangateOpenCellLibrary/NOR2_X${size}
  report_lib_cell NangateOpenCellLibrary/NOR3_X${size}
  report_lib_cell NangateOpenCellLibrary/NOR4_X${size}
}

############################################################
# Write liberty (exercises timing model writing)
############################################################

set outfile [make_result_file liberty_timing_models_write.lib]
sta::write_liberty NangateOpenCellLibrary $outfile

############################################################
# Read ASAP7 CCSN (CCS noise models)
############################################################

read_liberty ../../test/asap7_ccsn.lib.gz

############################################################
# Read latch library (exercises latch-specific paths)
############################################################

read_liberty ../../test/liberty_latch3.lib

############################################################
# Report check types to exercise more report paths
############################################################

report_check_types -max_slew -max_capacitance -max_fanout

############################################################
# ASAP7 cells with different timing model variations
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz

# Query AO/OA complex gates
read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz
