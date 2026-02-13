# Test timing arc/model queries and various cell types for code coverage
# Targets: TimingArc.cc, TableModel.cc, Liberty.cc (deep model queries),
#          LibertyReader.cc (timing type parsing), Sequential.cc
source ../../test/helpers.tcl

############################################################
# Read libraries with different timing model types
############################################################

read_liberty ../../test/nangate45/Nangate45_typ.lib
puts "PASS: read Nangate45"

read_verilog ../../sdc/test/sdc_test2.v
link_design sdc_test2

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_input_delay -clock clk1 2.0 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]
puts "PASS: basic setup"

############################################################
# Query timing arcs on various cell types
############################################################

# Combinational cells - report_lib_cell shows timing arcs
report_lib_cell NangateOpenCellLibrary/INV_X1
puts "PASS: report INV_X1"

report_lib_cell NangateOpenCellLibrary/BUF_X1
puts "PASS: report BUF_X1"

report_lib_cell NangateOpenCellLibrary/NAND2_X1
puts "PASS: report NAND2_X1"

report_lib_cell NangateOpenCellLibrary/NOR2_X1
puts "PASS: report NOR2_X1"

report_lib_cell NangateOpenCellLibrary/AOI21_X1
puts "PASS: report AOI21_X1"

report_lib_cell NangateOpenCellLibrary/OAI21_X1
puts "PASS: report OAI21_X1"

report_lib_cell NangateOpenCellLibrary/AND2_X1
puts "PASS: report AND2_X1"

report_lib_cell NangateOpenCellLibrary/OR2_X1
puts "PASS: report OR2_X1"

# XOR cells
report_lib_cell NangateOpenCellLibrary/XOR2_X1
puts "PASS: report XOR2_X1"

report_lib_cell NangateOpenCellLibrary/XNOR2_X1
puts "PASS: report XNOR2_X1"

# Full/Half adder
report_lib_cell NangateOpenCellLibrary/FA_X1
puts "PASS: report FA_X1"

report_lib_cell NangateOpenCellLibrary/HA_X1
puts "PASS: report HA_X1"

# MUX
report_lib_cell NangateOpenCellLibrary/MUX2_X1
puts "PASS: report MUX2_X1"

# Tristate
report_lib_cell NangateOpenCellLibrary/TINV_X1
puts "PASS: report TINV_X1"

############################################################
# Sequential cells (timing arcs: setup, hold, clk-to-q)
############################################################

report_lib_cell NangateOpenCellLibrary/DFF_X1
puts "PASS: report DFF_X1"

report_lib_cell NangateOpenCellLibrary/DFF_X2
puts "PASS: report DFF_X2"

report_lib_cell NangateOpenCellLibrary/DFFR_X1
puts "PASS: report DFFR_X1"

report_lib_cell NangateOpenCellLibrary/DFFS_X1
puts "PASS: report DFFS_X1"

report_lib_cell NangateOpenCellLibrary/DFFRS_X1
puts "PASS: report DFFRS_X1"

# Scan DFFs
report_lib_cell NangateOpenCellLibrary/SDFF_X1
puts "PASS: report SDFF_X1"

report_lib_cell NangateOpenCellLibrary/SDFFR_X1
puts "PASS: report SDFFR_X1"

report_lib_cell NangateOpenCellLibrary/SDFFS_X1
puts "PASS: report SDFFS_X1"

report_lib_cell NangateOpenCellLibrary/SDFFRS_X1
puts "PASS: report SDFFRS_X1"

# Latch
report_lib_cell NangateOpenCellLibrary/TLAT_X1
puts "PASS: report TLAT_X1"

# Clock gate
report_lib_cell NangateOpenCellLibrary/CLKGATETST_X1
puts "PASS: report CLKGATETST_X1"

report_lib_cell NangateOpenCellLibrary/CLKGATETST_X2
puts "PASS: report CLKGATETST_X2"

############################################################
# Query timing paths (exercises timing arc evaluation)
############################################################

report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: report_checks path 1"

report_checks -from [get_ports in2] -to [get_ports out1]
puts "PASS: report_checks path 2"

report_checks -from [get_ports in1] -to [get_ports out2]
puts "PASS: report_checks path 3"

# Min delay paths
report_checks -path_delay min -from [get_ports in1] -to [get_ports out1]
puts "PASS: report_checks min path"

# Rise/fall reports
report_checks -from [get_ports in1] -rise_to [get_ports out1]
puts "PASS: report_checks rise_to"

report_checks -from [get_ports in1] -fall_to [get_ports out1]
puts "PASS: report_checks fall_to"

############################################################
# Drive strength variations (larger cells with different tables)
############################################################

foreach size {1 2 4 8 16 32} {
  catch {
    report_lib_cell NangateOpenCellLibrary/INV_X${size}
  }
}
puts "PASS: INV drive strengths"

foreach size {1 2 4 8 16 32} {
  catch {
    report_lib_cell NangateOpenCellLibrary/BUF_X${size}
  }
}
puts "PASS: BUF drive strengths"

foreach size {1 2 4} {
  catch {
    report_lib_cell NangateOpenCellLibrary/NAND2_X${size}
    report_lib_cell NangateOpenCellLibrary/NAND3_X${size}
    report_lib_cell NangateOpenCellLibrary/NAND4_X${size}
    report_lib_cell NangateOpenCellLibrary/NOR2_X${size}
    report_lib_cell NangateOpenCellLibrary/NOR3_X${size}
    report_lib_cell NangateOpenCellLibrary/NOR4_X${size}
  }
}
puts "PASS: NAND/NOR drive strengths"

############################################################
# Write liberty (exercises timing model writing)
############################################################

set outfile [make_result_file liberty_timing_models_write.lib]
sta::write_liberty NangateOpenCellLibrary $outfile
puts "PASS: write_liberty timing models"

############################################################
# Read ASAP7 CCSN (CCS noise models)
############################################################

read_liberty ../../test/asap7_ccsn.lib.gz
puts "PASS: read CCSN library"

############################################################
# Read latch library (exercises latch-specific paths)
############################################################

read_liberty ../../test/liberty_latch3.lib
puts "PASS: read latch library"

############################################################
# Report check types to exercise more report paths
############################################################

report_check_types -max_slew -max_capacitance -max_fanout
puts "PASS: report_check_types"

############################################################
# ASAP7 cells with different timing model variations
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz
puts "PASS: read ASAP7 SIMPLE"

# Query AO/OA complex gates
read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz
puts "PASS: read ASAP7 AO"

read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz
puts "PASS: read ASAP7 OA"

puts "ALL PASSED"
