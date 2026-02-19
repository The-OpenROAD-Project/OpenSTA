# Test wire load model handling for code coverage
# Targets: Wireload.cc (WireloadSelection, wireloadTreeString, wireloadModeString),
#          LibertyReader.cc (wireload parsing), Liberty.cc (wireload queries),
#          LibertyWriter.cc (wireload writing)
source ../../test/helpers.tcl

############################################################
# Read library with wire load models
############################################################

read_liberty ../../test/nangate45/Nangate45_typ.lib

# Read verilog and link design to enable wireload operations
read_verilog ../../sdc/test/sdc_test2.v
link_design sdc_test2

############################################################
# Wire load model queries
############################################################

# Set various wire load models (Nangate has multiple)
# Nangate has 1K, 3K, 5K wireload models with different h/v ratios
set_wire_load_model -name "1K_hvratio_1_1"

set_wire_load_model -name "1K_hvratio_1_2"

set_wire_load_model -name "1K_hvratio_1_4"

set_wire_load_model -name "3K_hvratio_1_1"

set_wire_load_model -name "3K_hvratio_1_2"

set_wire_load_model -name "3K_hvratio_1_4"

set_wire_load_model -name "5K_hvratio_1_1"

set_wire_load_model -name "5K_hvratio_1_2"

set_wire_load_model -name "5K_hvratio_1_4"

############################################################
# Wire load mode switching (exercises wireloadModeString)
############################################################

set_wire_load_mode top

set_wire_load_mode enclosed

set_wire_load_mode segmented

############################################################
# Setup constraints and report
############################################################

create_clock -name clk1 -period 10 [get_ports clk1]
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_input_delay -clock clk1 2.0 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk1 3.0 [get_ports out2]

# Report checks with wire load
report_checks -from [get_ports in1] -to [get_ports out1]

############################################################
# Write SDC with wireload info
############################################################

set sdc_file [make_result_file liberty_wireload.sdc]
write_sdc -no_timestamp $sdc_file

############################################################
# Write liberty (exercises wireload writing in LibertyWriter)
############################################################

set outfile [make_result_file liberty_wireload_write.lib]
sta::write_liberty NangateOpenCellLibrary $outfile

############################################################
# Read Sky130 library (different wireload models)
############################################################

read_liberty ../../test/sky130hd/sky130hd_tt.lib

# Try Sky130 wire load models
set_wire_load_model -name "Small"
set_wire_load_model -name "Medium"

############################################################
# Write liberty for sky130 (different wireload format)
############################################################

set outfile2 [make_result_file liberty_wireload_write_sky130.lib]
sta::write_liberty sky130_fd_sc_hd__tt_025C_1v80 $outfile2

############################################################
# Read IHP library
############################################################

read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p20V_25C.lib

set outfile3 [make_result_file liberty_wireload_write_ihp.lib]
sta::write_liberty sg13g2_stdcell_typ_1p20V_25C $outfile3

############################################################
# Operating conditions + wireload interaction
############################################################

set_operating_conditions typical

report_checks -from [get_ports in1] -to [get_ports out1]
