# Test disable timing, case analysis, logic values, data checks
# Targets: Sdc.cc (setDisableTiming, case analysis, logic values,
#          setDataCheck, removeDataCheck, clockGatingMargin*),
#          DisabledPorts.cc, DataCheck.cc,
#          WriteSdc.cc (writeDisables, writeCaseAnalysis, writeConstants,
#                       writeDataChecks, writeClockGatingCheck)
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

# Setup
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_input_delay -clock clk2 2.0 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]

############################################################
# Disable timing - instances
############################################################

set_disable_timing [get_cells buf1]

set_disable_timing [get_cells inv1]

set_disable_timing [get_cells and1]

report_checks

# Unset and re-enable
unset_disable_timing [get_cells buf1]
unset_disable_timing [get_cells inv1]
unset_disable_timing [get_cells and1]

############################################################
# Disable timing - pins
############################################################

set_disable_timing [get_pins buf1/A]
set_disable_timing [get_pins inv1/A]
set_disable_timing [get_pins and1/A1]

report_checks

unset_disable_timing [get_pins buf1/A]
unset_disable_timing [get_pins inv1/A]
unset_disable_timing [get_pins and1/A1]

############################################################
# Disable timing - lib cells
############################################################

set_disable_timing [get_lib_cells NangateOpenCellLibrary/BUF_X1]

set_disable_timing [get_lib_cells NangateOpenCellLibrary/INV_X1] -from A -to ZN

set_disable_timing [get_lib_cells NangateOpenCellLibrary/NAND2_X1] -from A1 -to ZN

set_disable_timing [get_lib_cells NangateOpenCellLibrary/NOR2_X1]

set_disable_timing [get_lib_cells NangateOpenCellLibrary/AND2_X1] -from A1 -to ZN

# Write SDC with disable timing
set sdc_file1 [make_result_file sdc_disable1.sdc]
write_sdc -no_timestamp $sdc_file1
diff_files sdc_disable1.sdcok $sdc_file1

# Unset all lib cell disables
unset_disable_timing [get_lib_cells NangateOpenCellLibrary/BUF_X1]
unset_disable_timing [get_lib_cells NangateOpenCellLibrary/INV_X1] -from A -to ZN
unset_disable_timing [get_lib_cells NangateOpenCellLibrary/NAND2_X1] -from A1 -to ZN
unset_disable_timing [get_lib_cells NangateOpenCellLibrary/NOR2_X1]
unset_disable_timing [get_lib_cells NangateOpenCellLibrary/AND2_X1] -from A1 -to ZN

############################################################
# Case analysis
############################################################

# case_analysis 0
set_case_analysis 0 [get_ports in1]

# case_analysis 1
set_case_analysis 1 [get_ports in2]

# case_analysis rising
set_case_analysis rising [get_ports in3]

report_checks

# Write SDC with case analysis (exercises writeCaseAnalysis)
set sdc_file2 [make_result_file sdc_case1.sdc]
write_sdc -no_timestamp $sdc_file2
diff_files sdc_case1.sdcok $sdc_file2

# Unset case analysis
unset_case_analysis [get_ports in1]
unset_case_analysis [get_ports in2]
unset_case_analysis [get_ports in3]

# case_analysis falling
set_case_analysis falling [get_ports in1]

set sdc_file3 [make_result_file sdc_case2.sdc]
write_sdc -no_timestamp $sdc_file3
diff_files sdc_case2.sdcok $sdc_file3

unset_case_analysis [get_ports in1]

############################################################
# Logic values (set_logic_zero, set_logic_one, set_logic_dc)
############################################################

set_logic_zero [get_ports in1]

set_logic_one [get_ports in2]

# set_logic_dc (don't care)
set_logic_dc [get_ports in3]

# Write SDC with logic values (exercises writeConstants)
set sdc_file4 [make_result_file sdc_logic1.sdc]
write_sdc -no_timestamp $sdc_file4
diff_files sdc_logic1.sdcok $sdc_file4

report_checks

############################################################
# Data checks
############################################################

set_data_check -from [get_pins reg1/Q] -to [get_pins reg2/D] -setup 0.5

set_data_check -from [get_pins reg1/Q] -to [get_pins reg2/D] -hold 0.3

set_data_check -rise_from [get_pins reg1/Q] -to [get_pins reg2/D] -setup 0.6

set_data_check -from [get_pins reg1/Q] -fall_to [get_pins reg2/D] -hold 0.25

# Write with data checks
set sdc_file5 [make_result_file sdc_datacheck1.sdc]
write_sdc -no_timestamp $sdc_file5
diff_files sdc_datacheck1.sdcok $sdc_file5

# Remove data checks
unset_data_check -from [get_pins reg1/Q] -to [get_pins reg2/D] -setup

unset_data_check -from [get_pins reg1/Q] -to [get_pins reg2/D] -hold

############################################################
# Clock gating check (exercises clockGatingMargin paths)
############################################################

# Design-level
set_clock_gating_check -setup 0.5
set_clock_gating_check -hold 0.3

# Clock-level
set_clock_gating_check -setup 0.4 [get_clocks clk1]
set_clock_gating_check -hold 0.2 [get_clocks clk1]
set_clock_gating_check -setup 0.35 [get_clocks clk2]
set_clock_gating_check -hold 0.15 [get_clocks clk2]

# Instance-level
set_clock_gating_check -setup 0.3 [get_cells reg1]
set_clock_gating_check -hold 0.1 [get_cells reg1]

# Pin-level
set_clock_gating_check -setup 0.25 [get_pins reg1/CK]
set_clock_gating_check -hold 0.08 [get_pins reg1/CK]

# Write SDC with clock gating
set sdc_file6 [make_result_file sdc_clkgate1.sdc]
write_sdc -no_timestamp $sdc_file6
diff_files sdc_clkgate1.sdcok $sdc_file6

############################################################
# set_ideal_network / set_ideal_transition
############################################################

set_ideal_network [get_ports clk1]
set_ideal_network [get_ports clk2]

set_ideal_transition 0.0 [get_ports clk1]
set_ideal_transition 0.05 [get_ports clk2]

############################################################
# Min pulse width on various objects
############################################################

set_min_pulse_width 1.0 [get_clocks clk1]
set_min_pulse_width -high 0.6 [get_clocks clk1]
set_min_pulse_width -low 0.4 [get_clocks clk1]

set_min_pulse_width 0.8 [get_clocks clk2]

set_min_pulse_width 0.5 [get_pins reg1/CK]

set_min_pulse_width 0.6 [get_cells reg1]

############################################################
# Latch borrow limits on various objects
############################################################

set_max_time_borrow 2.0 [get_clocks clk1]
set_max_time_borrow 1.5 [get_clocks clk2]

set_max_time_borrow 1.0 [get_pins reg1/D]

set_max_time_borrow 1.2 [get_cells reg2]

############################################################
# Comprehensive write with all constraints
############################################################

set sdc_final [make_result_file sdc_disable_case_final.sdc]
write_sdc -no_timestamp $sdc_final
diff_files sdc_disable_case_final.sdcok $sdc_final

set sdc_compat [make_result_file sdc_disable_case_compat.sdc]
write_sdc -no_timestamp -compatible $sdc_compat
diff_files sdc_disable_case_compat.sdcok $sdc_compat

set sdc_d6 [make_result_file sdc_disable_case_d6.sdc]
write_sdc -no_timestamp -digits 6 $sdc_d6
diff_files sdc_disable_case_d6.sdcok $sdc_d6

report_checks
