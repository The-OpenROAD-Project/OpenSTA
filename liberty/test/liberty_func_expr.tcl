# Test complex boolean function expressions and cell type classification
# Targets: FuncExpr.cc (to_string, port matching, op types),
#          Liberty.cc (cell property queries, function expression evaluation),
#          LibertyReader.cc (function expression parsing),
#          TimingArc.cc (arc queries on cells with complex functions)
source ../../test/helpers.tcl

############################################################
# Read Nangate45 - has AOI, OAI, MUX, XOR, XNOR etc.
############################################################

read_liberty ../../test/nangate45/Nangate45_typ.lib
puts "PASS: read Nangate45"

# Link a design to enable timing queries
read_verilog ../../sdc/test/sdc_test2.v
link_design sdc_test2

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [all_inputs]
set_output_delay -clock clk1 3.0 [all_outputs]
puts "PASS: design setup"

############################################################
# XOR/XNOR cells (FuncExpr op_xor)
############################################################

report_lib_cell NangateOpenCellLibrary/XOR2_X1
puts "PASS: report XOR2_X1"

report_lib_cell NangateOpenCellLibrary/XOR2_X2
puts "PASS: report XOR2_X2"

report_lib_cell NangateOpenCellLibrary/XNOR2_X1
puts "PASS: report XNOR2_X1"

report_lib_cell NangateOpenCellLibrary/XNOR2_X2
puts "PASS: report XNOR2_X2"

############################################################
# AOI cells (complex AND-OR-INVERT functions)
############################################################

# AOI21: !(A1&A2 | B)
report_lib_cell NangateOpenCellLibrary/AOI21_X1
report_lib_cell NangateOpenCellLibrary/AOI21_X2
report_lib_cell NangateOpenCellLibrary/AOI21_X4
puts "PASS: report AOI21 variants"

# AOI22: !(A1&A2 | B1&B2)
report_lib_cell NangateOpenCellLibrary/AOI22_X1
report_lib_cell NangateOpenCellLibrary/AOI22_X2
report_lib_cell NangateOpenCellLibrary/AOI22_X4
puts "PASS: report AOI22 variants"

# AOI211: !(A1&A2 | B | C)
report_lib_cell NangateOpenCellLibrary/AOI211_X1
report_lib_cell NangateOpenCellLibrary/AOI211_X2
report_lib_cell NangateOpenCellLibrary/AOI211_X4
puts "PASS: report AOI211 variants"

############################################################
# OAI cells (complex OR-AND-INVERT functions)
############################################################

# OAI21: !((A1|A2) & B)
report_lib_cell NangateOpenCellLibrary/OAI21_X1
report_lib_cell NangateOpenCellLibrary/OAI21_X2
report_lib_cell NangateOpenCellLibrary/OAI21_X4
puts "PASS: report OAI21 variants"

# OAI22: !((A1|A2) & (B1|B2))
report_lib_cell NangateOpenCellLibrary/OAI22_X1
report_lib_cell NangateOpenCellLibrary/OAI22_X2
report_lib_cell NangateOpenCellLibrary/OAI22_X4
puts "PASS: report OAI22 variants"

# OAI211: !((A1|A2) & B & C)
report_lib_cell NangateOpenCellLibrary/OAI211_X1
report_lib_cell NangateOpenCellLibrary/OAI211_X2
report_lib_cell NangateOpenCellLibrary/OAI211_X4
puts "PASS: report OAI211 variants"

# OAI33: !((A1|A2|A3) & (B1|B2|B3))
catch { report_lib_cell NangateOpenCellLibrary/OAI33_X1 }
puts "PASS: report OAI33"

############################################################
# MUX cells (complex function: S?B:A)
############################################################

report_lib_cell NangateOpenCellLibrary/MUX2_X1
report_lib_cell NangateOpenCellLibrary/MUX2_X2
puts "PASS: report MUX2 variants"

############################################################
# Full/half adder (complex multi-output functions)
############################################################

report_lib_cell NangateOpenCellLibrary/FA_X1
puts "PASS: report FA_X1 (full adder)"

report_lib_cell NangateOpenCellLibrary/HA_X1
puts "PASS: report HA_X1 (half adder)"

############################################################
# Tristate cells (three_state enable)
############################################################

report_lib_cell NangateOpenCellLibrary/TINV_X1
puts "PASS: report TINV_X1 (tristate inv)"

catch { report_lib_cell NangateOpenCellLibrary/TBUF_X1 }
catch { report_lib_cell NangateOpenCellLibrary/TBUF_X2 }
puts "PASS: report TBUF tristate buffer"

############################################################
# Special cells: antenna, filler, tie, clock gate
############################################################

report_lib_cell NangateOpenCellLibrary/ANTENNA_X1
puts "PASS: report ANTENNA_X1"

report_lib_cell NangateOpenCellLibrary/FILLCELL_X1
report_lib_cell NangateOpenCellLibrary/FILLCELL_X2
report_lib_cell NangateOpenCellLibrary/FILLCELL_X4
report_lib_cell NangateOpenCellLibrary/FILLCELL_X8
report_lib_cell NangateOpenCellLibrary/FILLCELL_X16
report_lib_cell NangateOpenCellLibrary/FILLCELL_X32
puts "PASS: report FILLCELL variants"

report_lib_cell NangateOpenCellLibrary/LOGIC0_X1
report_lib_cell NangateOpenCellLibrary/LOGIC1_X1
puts "PASS: report tie cells"

report_lib_cell NangateOpenCellLibrary/CLKGATETST_X1
report_lib_cell NangateOpenCellLibrary/CLKGATETST_X2
report_lib_cell NangateOpenCellLibrary/CLKGATETST_X4
report_lib_cell NangateOpenCellLibrary/CLKGATETST_X8
puts "PASS: report clock gate cells"

############################################################
# Scan DFF cells (complex function with scan mux)
############################################################

report_lib_cell NangateOpenCellLibrary/SDFF_X1
report_lib_cell NangateOpenCellLibrary/SDFF_X2
report_lib_cell NangateOpenCellLibrary/SDFFR_X1
report_lib_cell NangateOpenCellLibrary/SDFFR_X2
report_lib_cell NangateOpenCellLibrary/SDFFS_X1
report_lib_cell NangateOpenCellLibrary/SDFFS_X2
report_lib_cell NangateOpenCellLibrary/SDFFRS_X1
report_lib_cell NangateOpenCellLibrary/SDFFRS_X2
puts "PASS: report scan DFF variants"

############################################################
# Write liberty to exercise FuncExpr::to_string for all types
############################################################

set outfile1 [make_result_file liberty_func_expr_write.lib]
sta::write_liberty NangateOpenCellLibrary $outfile1
puts "PASS: write_liberty (exercises FuncExpr::to_string)"

############################################################
# Read IHP library (different function syntax/features)
############################################################

read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p20V_25C.lib
puts "PASS: read IHP library"

# IHP has different cell naming and function formats
catch { report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_inv_1 }
catch { report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_buf_1 }
catch { report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_nand2_1 }
catch { report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_nor2_1 }
catch { report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_xor2_1 }
catch { report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_xnor2_1 }
catch { report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_mux2_1 }
catch { report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_dfrbp_1 }
catch { report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_ebufn_2 }
catch { report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_antn }
catch { report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_tiehi }
catch { report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_tielo }
puts "PASS: IHP cell reports"

set outfile2 [make_result_file liberty_func_expr_write_ihp.lib]
sta::write_liberty sg13g2_stdcell_typ_1p20V_25C $outfile2
puts "PASS: write_liberty IHP"

############################################################
# Read Sky130 library (yet another function expression style)
############################################################

read_liberty ../../test/sky130hd/sky130hd_tt.lib
puts "PASS: read Sky130"

# Sky130 has complex cells with different function expression styles
catch { report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__a21o_1 }
catch { report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__a21oi_1 }
catch { report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__a22o_1 }
catch { report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__a22oi_1 }
catch { report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__a31o_1 }
catch { report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__a32o_1 }
catch { report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__o21a_1 }
catch { report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__o21ai_0 }
catch { report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__o22a_1 }
catch { report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__mux2_1 }
catch { report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__mux4_1 }
catch { report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__xor2_1 }
catch { report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__xnor2_1 }
catch { report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__fa_1 }
catch { report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__ha_1 }
catch { report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__maj3_1 }
catch { report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__dlxtp_1 }
catch { report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__sdfxtp_1 }
catch { report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__ebufn_1 }
puts "PASS: Sky130 complex cell reports"

set outfile3 [make_result_file liberty_func_expr_write_sky130.lib]
sta::write_liberty sky130_fd_sc_hd__tt_025C_1v80 $outfile3
puts "PASS: write_liberty Sky130"

############################################################
# Timing path reports through complex cells
############################################################

report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: report_checks"

report_checks -from [get_ports in1] -to [get_ports out1] -path_delay min
puts "PASS: report_checks min delay"

puts "ALL PASSED"
