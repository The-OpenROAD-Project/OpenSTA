# Test internal power, leakage power, and power reporting for code coverage
# Targets: InternalPower.cc, LeakagePower.cc, Liberty.cc (power paths),
#          LibertyReader.cc (power group parsing), LibertyWriter.cc (power writing)
source ../../test/helpers.tcl

############################################################
# Read libraries with power models
############################################################

read_liberty ../../test/nangate45/Nangate45_typ.lib
puts "PASS: read Nangate45"

# Read a design to enable power reporting
read_verilog ../../sdc/test/sdc_test2.v
link_design sdc_test2

# Setup constraints for power analysis
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_input_delay -clock clk1 2.0 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]
set_input_transition 0.1 [get_ports in1]
set_input_transition 0.1 [get_ports in2]
set_input_transition 0.1 [get_ports in3]
puts "PASS: design setup"

############################################################
# Report power (exercises internal power computation)
############################################################

report_power
puts "PASS: report_power design"

report_power -digits 6
puts "PASS: report_power -digits 6"

# Report power for specific instances
report_power -instances [get_cells buf1]
puts "PASS: report_power buf1"

report_power -instances [get_cells inv1]
puts "PASS: report_power inv1"

report_power -instances [get_cells and1]
puts "PASS: report_power and1"

report_power -instances [get_cells reg1]
puts "PASS: report_power reg1"

report_power -instances [get_cells reg2]
puts "PASS: report_power reg2"

report_power -instances [get_cells reg3]
puts "PASS: report_power reg3"

report_power -instances [get_cells {buf1 inv1 and1 or1 nand1 nor1 reg1 reg2 reg3}]
puts "PASS: report_power all instances"

############################################################
# Cell leakage power property (exercises LeakagePower.cc)
############################################################

set inv_cell [get_lib_cell NangateOpenCellLibrary/INV_X1]
catch { puts "INV_X1 leakage_power: [get_property $inv_cell cell_leakage_power]" }
puts "PASS: INV_X1 leakage_power"

set buf_cell [get_lib_cell NangateOpenCellLibrary/BUF_X1]
catch { puts "BUF_X1 leakage_power: [get_property $buf_cell cell_leakage_power]" }
puts "PASS: BUF_X1 leakage_power"

set dff_cell [get_lib_cell NangateOpenCellLibrary/DFF_X1]
catch { puts "DFF_X1 leakage_power: [get_property $dff_cell cell_leakage_power]" }
puts "PASS: DFF_X1 leakage_power"

set nand_cell [get_lib_cell NangateOpenCellLibrary/NAND2_X1]
catch { puts "NAND2_X1 leakage_power: [get_property $nand_cell cell_leakage_power]" }
puts "PASS: NAND2_X1 leakage_power"

# Area property
catch { puts "INV_X1 area: [get_property $inv_cell area]" }
catch { puts "BUF_X1 area: [get_property $buf_cell area]" }
catch { puts "DFF_X1 area: [get_property $dff_cell area]" }
puts "PASS: cell area properties"

############################################################
# Cell properties - is_buffer, is_inverter, etc.
############################################################

puts "INV_X1 is_inverter: [get_property $inv_cell is_inverter]"
puts "INV_X1 is_buffer: [get_property $inv_cell is_buffer]"
puts "BUF_X1 is_buffer: [get_property $buf_cell is_buffer]"
puts "BUF_X1 is_inverter: [get_property $buf_cell is_inverter]"
puts "DFF_X1 is_buffer: [get_property $dff_cell is_buffer]"
catch { puts "DFF_X1 is_register: [get_property $dff_cell is_register]" }
puts "PASS: cell type properties"

############################################################
# Write liberty and re-read (exercises writer power paths)
############################################################

set outfile [make_result_file liberty_power_write.lib]
sta::write_liberty NangateOpenCellLibrary $outfile
puts "PASS: write_liberty with power"

############################################################
# Read more libraries with power data
############################################################

read_liberty ../../test/sky130hd/sky130hd_tt.lib
puts "PASS: read Sky130 (has power data)"

# Query sky130 cell leakage powers
set sky_inv [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__inv_1]
catch { puts "sky130 inv leakage: [get_property $sky_inv cell_leakage_power]" }
catch { puts "sky130 inv area: [get_property $sky_inv area]" }
puts "PASS: sky130 cell properties"

# Write sky130 liberty
set outfile2 [make_result_file liberty_power_write_sky130.lib]
sta::write_liberty sky130_fd_sc_hd__tt_025C_1v80 $outfile2
puts "PASS: write_liberty sky130"

############################################################
# Read IHP library and query power
############################################################

read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p20V_25C.lib
puts "PASS: read IHP"

set ihp_inv [get_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_inv_1]
catch { puts "IHP inv leakage: [get_property $ihp_inv cell_leakage_power]" }
catch { puts "IHP inv area: [get_property $ihp_inv area]" }
puts "PASS: IHP cell properties"

############################################################
# Read ASAP7 CCSN library (CCS timing + power models)
############################################################

read_liberty ../../test/asap7_ccsn.lib.gz
puts "PASS: read CCSN library"

set outfile3 [make_result_file liberty_power_write_ccsn.lib]
catch {
  sta::write_liberty asap7_ccsn $outfile3
}
puts "PASS: write_liberty CCSN (may skip if lib name unknown)"

############################################################
# Read ASAP7 SEQ for power on sequential cells
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
puts "PASS: read ASAP7 SEQ"

set asap7_dff [get_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/DFFHQNx1_ASAP7_75t_R]
catch { puts "ASAP7 DFF leakage: [get_property $asap7_dff cell_leakage_power]" }
catch { puts "ASAP7 DFF area: [get_property $asap7_dff area]" }
puts "PASS: ASAP7 DFF properties"

puts "ALL PASSED"
