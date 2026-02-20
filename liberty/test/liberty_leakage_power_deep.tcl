# Test deep leakage power groups with when conditions, internal power
# with related_pg_pin, and power attribute parsing across PDKs.
source ../../test/helpers.tcl

############################################################
# Read Sky130 library (has leakage_power groups with when conditions)
############################################################
read_liberty ../../test/sky130hd/sky130hd_tt.lib

############################################################
# Query leakage power on various cell types
# Sky130 has per-state leakage_power groups with when conditions
############################################################
puts "--- leakage power queries ---"

# Note: cell_leakage_power is not a supported get_property property.
# Leakage power is exercised through report_power and report_lib_cell below.

############################################################
# Report lib cells to exercise detailed leakage/power info
############################################################
puts "--- detailed cell reports ---"

report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__inv_1

report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__nand2_1

report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__dfxtp_1

############################################################
# Read Nangate library for internal power with when conditions
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib

############################################################
# Link design and run power analysis to exercise internal power
############################################################
read_verilog ../../sdc/test/sdc_test2.v
link_design sdc_test2

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [all_inputs]
set_output_delay -clock clk1 3.0 [all_outputs]
set_input_transition 0.1 [all_inputs]

# Power reports exercise internal power evaluation
report_power

report_power -digits 8

# Per-instance power
foreach inst_name {buf1 inv1 and1 or1 nand1 nor1 reg1 reg2 reg3} {
  report_power -instances [get_cells $inst_name]
}

############################################################
# Read IHP library for different power model format
############################################################
read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p20V_25C.lib

############################################################
# Write liberty roundtrip for Sky130 (exercises power writer)
############################################################
set outfile [make_result_file liberty_leakage_power_deep_write.lib]
  sta::write_liberty sky130_fd_sc_hd__tt_025C_1v80 $outfile
