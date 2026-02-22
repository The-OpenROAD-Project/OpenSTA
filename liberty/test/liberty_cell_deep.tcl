# Deep cell property queries: timing arc traversal, port capacitance,
# sequential elements, leakage/internal power, and cell classification.
source ../../test/helpers.tcl

############################################################
# Read libraries
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib

read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib

read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p20V_25C.lib

############################################################
# Port capacitance queries
# Exercises: LibertyPort capacitance getters/setters
############################################################
set inv_a [get_lib_pin NangateOpenCellLibrary/INV_X1/A]
set inv_zn [get_lib_pin NangateOpenCellLibrary/INV_X1/ZN]

set cap_a [get_property $inv_a capacitance]

set cap_zn [get_property $inv_zn capacitance]

# DFF capacitance queries
set dff_ck [get_lib_pin NangateOpenCellLibrary/DFF_X1/CK]
set cap_ck [get_property $dff_ck capacitance]

set dff_d [get_lib_pin NangateOpenCellLibrary/DFF_X1/D]
set cap_d [get_property $dff_d capacitance]

# Larger drive strengths have different capacitances
foreach size {1 2 4 8 16 32} {
  set pin [get_lib_pin NangateOpenCellLibrary/INV_X${size}/A]
  set cap [get_property $pin capacitance]
  puts "INV_X${size}/A cap = $cap"
}

foreach size {1 2 4 8 16 32} {
  set pin [get_lib_pin NangateOpenCellLibrary/BUF_X${size}/A]
  set cap [get_property $pin capacitance]
  puts "BUF_X${size}/A cap = $cap"
}

############################################################
# Cell area queries
############################################################
foreach cell_name {INV_X1 INV_X2 INV_X4 INV_X8 INV_X16 INV_X32
                   BUF_X1 BUF_X2 BUF_X4 BUF_X8 BUF_X16 BUF_X32
                   DFF_X1 DFF_X2 DFFR_X1 DFFS_X1 DFFRS_X1
                   NAND2_X1 NAND2_X2 NAND2_X4
                   NOR2_X1 NOR2_X2 NOR2_X4
                   AOI21_X1 OAI21_X1 MUX2_X1 FA_X1 HA_X1
                   TINV_X1 CLKGATETST_X1} {
  set cell [get_lib_cell NangateOpenCellLibrary/$cell_name]
  set area [get_property $cell area]
  puts "$cell_name area = $area"
}

############################################################
# Cell dont_use, is_macro, is_memory queries
############################################################
foreach cell_name {INV_X1 BUF_X1 DFF_X1 ANTENNA_X1 FILLCELL_X1} {
  set cell [get_lib_cell NangateOpenCellLibrary/$cell_name]
  set du [get_property $cell dont_use]
  puts "$cell_name dont_use = $du"
}

############################################################
# Leakage power queries
############################################################

############################################################
# Timing arc property queries
# Exercises: arc direction, sense, timing_type
############################################################

# Setup a design and run timing to exercise arc evaluation
read_verilog ../../sdc/test/sdc_test2.v
link_design sdc_test2
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [all_inputs]
set_output_delay -clock clk1 3.0 [all_outputs]
set_input_transition 0.1 [all_inputs]

# Detailed timing reports exercise arc evaluation
report_checks -from [get_ports in1] -to [get_ports out1] -path_delay max

report_checks -from [get_ports in1] -to [get_ports out1] -path_delay min

report_checks -from [get_ports in2] -to [get_ports out2]

# Rise/fall reports exercise different arc transitions
report_checks -rise_from [get_ports in1] -to [get_ports out1]

report_checks -fall_from [get_ports in1] -to [get_ports out1]

report_checks -from [get_ports in1] -rise_to [get_ports out1]

report_checks -from [get_ports in1] -fall_to [get_ports out1]

############################################################
# Report check types exercises different check arc types
############################################################
report_check_types -max_delay -min_delay

report_check_types -max_slew -max_capacitance -max_fanout

report_check_types -recovery -removal

report_check_types -min_pulse_width -min_period

report_check_types -clock_gating_setup -clock_gating_hold

report_check_types -max_skew

############################################################
# Report power to exercise internal power model paths
############################################################
report_power

report_power -instances [get_cells *]

############################################################
# Sky130 cells - different tristate and latch cells
############################################################
read_liberty ../../test/sky130hd/sky130hd_tt.lib

# Tristate buffer
report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__ebufn_1
report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__ebufn_2
report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__ebufn_4

# Latch cells
report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__dlxtp_1
report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__dlxtn_1

# Scan flip-flops
report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__sdfxtp_1
report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__sdfxbp_1

# DFF with async set/clear (exercises recovery/removal)
report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__dfxtp_1
report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__dfrtp_1
report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__dfstp_1
report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__dfbbp_1

# Mux cells
report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__mux2_1
report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__mux2i_1
report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__mux4_1

############################################################
# Write roundtrip to exercise all writer cell/arc/model paths
############################################################
set outfile [make_result_file liberty_cell_deep_write.lib]
sta::write_liberty NangateOpenCellLibrary $outfile

# Verify file was written
puts "write_liberty output: [file size $outfile] bytes"
