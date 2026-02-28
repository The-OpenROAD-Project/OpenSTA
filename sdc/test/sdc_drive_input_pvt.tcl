# Test input drive, driving cell, input slew, PVT, operating conditions,
# analysis type, and their write_sdc paths.
# Targets:
#   InputDrive.cc: setSlew, setDriveResistance, driveResistance,
#     hasDriveResistance, driveResistanceMinMaxEqual,
#     setDriveCell, driveCell, hasDriveCell, driveCellsEqual,
#     slew, InputDriveCell methods (equal, setLibrary, setCell,
#     setFromPort, setToPort, setFromSlews)
#   Sdc.cc: setInputSlew, setDriveResistance, setDriveCell,
#     setOperatingConditions, setAnalysisType, setPvt,
#     findInputDrive, operatingConditions
#   WriteSdc.cc: writeDriveResistances, writeDrivingCells,
#     writeDrivingCell, writeInputTransitions,
#     writeOperatingConditions, writeVariables
#   Sdc.i: set_analysis_type_cmd, set_operating_conditions_cmd,
#     set_instance_pvt, operating_condition_analysis_type,
#     set_drive_cell_cmd, set_drive_resistance_cmd,
#     set_input_slew_cmd
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_input_delay -clock clk2 2.0 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]

############################################################
# Input transition / slew
############################################################
set_input_transition 0.15 [get_ports in1]
set_input_transition -rise -max 0.12 [get_ports in2]
set_input_transition -fall -min 0.08 [get_ports in2]
set_input_transition -rise 0.10 [get_ports in3]
set_input_transition -fall 0.11 [get_ports in3]
set_input_transition -rise -min 0.06 [get_ports in1]
set_input_transition -fall -max 0.18 [get_ports in1]
report_checks

############################################################
# Drive resistance
############################################################
set_drive 100 [get_ports in1]

set_drive -rise 120 [get_ports in2]
set_drive -fall 130 [get_ports in2]

set_drive -rise -min 80 [get_ports in3]
set_drive -rise -max 100 [get_ports in3]
set_drive -fall -min 90 [get_ports in3]
set_drive -fall -max 110 [get_ports in3]

report_checks

############################################################
# Driving cells - basic
############################################################
set_driving_cell -lib_cell BUF_X1 [get_ports in1]

set_driving_cell -lib_cell INV_X1 -pin ZN [get_ports in2]

# Driving cell with -from_pin
set_driving_cell -lib_cell AND2_X1 -from_pin A1 -pin ZN [get_ports in3]

# Driving cell with input transition slews
set_driving_cell -lib_cell BUF_X4 -pin Z \
    -input_transition_rise 0.05 -input_transition_fall 0.06 \
    [get_ports in1]

# Driving cell with -library
set_driving_cell -library NangateOpenCellLibrary -lib_cell INV_X2 -pin ZN \
    [get_ports in2]

# Driving cell with rise/fall
set_driving_cell -lib_cell BUF_X2 -pin Z -rise [get_ports in3]
set_driving_cell -lib_cell BUF_X8 -pin Z -fall [get_ports in3]

# Driving cell with min/max
set_driving_cell -lib_cell INV_X4 -pin ZN -min [get_ports in1]
set_driving_cell -lib_cell INV_X8 -pin ZN -max [get_ports in1]

report_checks

############################################################
# Write SDC - exercises writing drive resistance and driving cell
############################################################
set sdc1 [make_result_file sdc_drive_input1.sdc]
write_sdc -no_timestamp $sdc1

set sdc2 [make_result_file sdc_drive_input2.sdc]
write_sdc -no_timestamp -compatible $sdc2

############################################################
# Operating conditions
############################################################
set_operating_conditions typical

report_checks

############################################################
# Analysis type
############################################################
sta::set_analysis_type_cmd single

sta::set_analysis_type_cmd bc_wc

sta::set_analysis_type_cmd on_chip_variation

# Reset to single
sta::set_analysis_type_cmd single

# Analysis type through set_operating_conditions
set_operating_conditions -analysis_type bc_wc

set_operating_conditions -analysis_type single

############################################################
# PVT settings on instances
############################################################
set_pvt [get_cells buf1] -process 1.0 -voltage 1.1 -temperature 25.0

set_pvt [get_cells inv1] -process 0.9 -voltage 1.0 -temperature 85.0

############################################################
# Wire load model and mode
############################################################
set_wire_load_model -name "5K_hvratio_1_1"

set_wire_load_mode enclosed

set_wire_load_mode top

set_wire_load_mode segmented

############################################################
# Propagate all clocks variable
############################################################
sta::set_propagate_all_clocks 1

############################################################
# Write after all environment settings
############################################################
set sdc3 [make_result_file sdc_drive_input3.sdc]
write_sdc -no_timestamp $sdc3

# Write with digits
set sdc4 [make_result_file sdc_drive_input4.sdc]
write_sdc -no_timestamp -digits 6 $sdc4

############################################################
# Read back and verify
############################################################
read_sdc $sdc1
report_checks

############################################################
# Port external capacitance
############################################################
set_load -pin_load 0.05 [get_ports out1]
set_load -wire_load 0.02 [get_ports out1]
set_load -pin_load -rise 0.04 [get_ports out2]
set_load -pin_load -fall 0.045 [get_ports out2]

set_port_fanout_number 4 [get_ports out1]
set_port_fanout_number 6 [get_ports out2]

############################################################
# Final write
############################################################
set sdc5 [make_result_file sdc_drive_input5.sdc]
write_sdc -no_timestamp $sdc5
