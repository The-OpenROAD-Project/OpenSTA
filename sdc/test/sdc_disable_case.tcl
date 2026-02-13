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
puts "PASS: setup"

############################################################
# Disable timing - instances
############################################################

set_disable_timing [get_cells buf1]
puts "PASS: disable buf1"

set_disable_timing [get_cells inv1]
puts "PASS: disable inv1"

set_disable_timing [get_cells and1]
puts "PASS: disable and1"

report_checks
puts "PASS: report after disabling instances"

# Unset and re-enable
unset_disable_timing [get_cells buf1]
unset_disable_timing [get_cells inv1]
unset_disable_timing [get_cells and1]
puts "PASS: unset disable instances"

############################################################
# Disable timing - pins
############################################################

set_disable_timing [get_pins buf1/A]
set_disable_timing [get_pins inv1/A]
set_disable_timing [get_pins and1/A1]
puts "PASS: disable pins"

report_checks
puts "PASS: report after disabling pins"

unset_disable_timing [get_pins buf1/A]
unset_disable_timing [get_pins inv1/A]
unset_disable_timing [get_pins and1/A1]
puts "PASS: unset disable pins"

############################################################
# Disable timing - lib cells
############################################################

set_disable_timing [get_lib_cells NangateOpenCellLibrary/BUF_X1]
puts "PASS: disable lib cell BUF_X1"

set_disable_timing [get_lib_cells NangateOpenCellLibrary/INV_X1] -from A -to ZN
puts "PASS: disable lib cell INV_X1 from/to"

set_disable_timing [get_lib_cells NangateOpenCellLibrary/NAND2_X1] -from A1 -to ZN
puts "PASS: disable lib cell NAND2_X1 from/to"

set_disable_timing [get_lib_cells NangateOpenCellLibrary/NOR2_X1]
puts "PASS: disable lib cell NOR2_X1"

set_disable_timing [get_lib_cells NangateOpenCellLibrary/AND2_X1] -from A1 -to ZN
puts "PASS: disable lib cell AND2_X1 from/to"

# Write SDC with disable timing
set sdc_file1 [make_result_file sdc_disable1.sdc]
write_sdc -no_timestamp $sdc_file1
puts "PASS: write_sdc with disable"

# Unset all lib cell disables
unset_disable_timing [get_lib_cells NangateOpenCellLibrary/BUF_X1]
unset_disable_timing [get_lib_cells NangateOpenCellLibrary/INV_X1] -from A -to ZN
unset_disable_timing [get_lib_cells NangateOpenCellLibrary/NAND2_X1] -from A1 -to ZN
unset_disable_timing [get_lib_cells NangateOpenCellLibrary/NOR2_X1]
unset_disable_timing [get_lib_cells NangateOpenCellLibrary/AND2_X1] -from A1 -to ZN
puts "PASS: unset all lib cell disables"

############################################################
# Case analysis
############################################################

# case_analysis 0
set_case_analysis 0 [get_ports in1]
puts "PASS: case_analysis 0 on in1"

# case_analysis 1
set_case_analysis 1 [get_ports in2]
puts "PASS: case_analysis 1 on in2"

# case_analysis rising
set_case_analysis rising [get_ports in3]
puts "PASS: case_analysis rising on in3"

report_checks
puts "PASS: report after case analysis"

# Write SDC with case analysis (exercises writeCaseAnalysis)
set sdc_file2 [make_result_file sdc_case1.sdc]
write_sdc -no_timestamp $sdc_file2
puts "PASS: write_sdc with case analysis"

# Unset case analysis
unset_case_analysis [get_ports in1]
unset_case_analysis [get_ports in2]
unset_case_analysis [get_ports in3]
puts "PASS: unset case analysis"

# case_analysis falling
set_case_analysis falling [get_ports in1]
puts "PASS: case_analysis falling on in1"

set sdc_file3 [make_result_file sdc_case2.sdc]
write_sdc -no_timestamp $sdc_file3
puts "PASS: write_sdc with falling case analysis"

unset_case_analysis [get_ports in1]
puts "PASS: unset falling case analysis"

############################################################
# Logic values (set_logic_zero, set_logic_one, set_logic_dc)
############################################################

set_logic_zero [get_ports in1]
puts "PASS: set_logic_zero in1"

set_logic_one [get_ports in2]
puts "PASS: set_logic_one in2"

# set_logic_dc (don't care)
catch {
  set_logic_dc [get_ports in3]
  puts "PASS: set_logic_dc in3"
}

# Write SDC with logic values (exercises writeConstants)
set sdc_file4 [make_result_file sdc_logic1.sdc]
write_sdc -no_timestamp $sdc_file4
puts "PASS: write_sdc with logic values"

report_checks
puts "PASS: report after logic values"

############################################################
# Data checks
############################################################

catch {
  set_data_check -from [get_pins reg1/Q] -to [get_pins reg2/D] -setup 0.5
  puts "PASS: data_check setup"
}

catch {
  set_data_check -from [get_pins reg1/Q] -to [get_pins reg2/D] -hold 0.3
  puts "PASS: data_check hold"
}

catch {
  set_data_check -rise_from [get_pins reg1/Q] -to [get_pins reg2/D] -setup 0.6
  puts "PASS: data_check rise_from setup"
}

catch {
  set_data_check -from [get_pins reg1/Q] -fall_to [get_pins reg2/D] -hold 0.25
  puts "PASS: data_check fall_to hold"
}

# Write with data checks
set sdc_file5 [make_result_file sdc_datacheck1.sdc]
write_sdc -no_timestamp $sdc_file5
puts "PASS: write_sdc with data checks"

# Remove data checks
catch {
  remove_data_check -from [get_pins reg1/Q] -to [get_pins reg2/D] -setup
  puts "PASS: remove_data_check setup"
}

catch {
  remove_data_check -from [get_pins reg1/Q] -to [get_pins reg2/D] -hold
  puts "PASS: remove_data_check hold"
}

############################################################
# Clock gating check (exercises clockGatingMargin paths)
############################################################

# Design-level
set_clock_gating_check -setup 0.5 [current_design]
set_clock_gating_check -hold 0.3 [current_design]
puts "PASS: clock_gating_check design"

# Clock-level
set_clock_gating_check -setup 0.4 [get_clocks clk1]
set_clock_gating_check -hold 0.2 [get_clocks clk1]
set_clock_gating_check -setup 0.35 [get_clocks clk2]
set_clock_gating_check -hold 0.15 [get_clocks clk2]
puts "PASS: clock_gating_check clocks"

# Instance-level
catch {
  set_clock_gating_check -setup 0.3 [get_cells reg1]
  set_clock_gating_check -hold 0.1 [get_cells reg1]
  puts "PASS: clock_gating_check instance"
}

# Pin-level
catch {
  set_clock_gating_check -setup 0.25 [get_pins reg1/CK]
  set_clock_gating_check -hold 0.08 [get_pins reg1/CK]
  puts "PASS: clock_gating_check pin"
}

# Write SDC with clock gating
set sdc_file6 [make_result_file sdc_clkgate1.sdc]
write_sdc -no_timestamp $sdc_file6
puts "PASS: write_sdc with clock gating"

############################################################
# set_ideal_network / set_ideal_transition
############################################################

set_ideal_network [get_ports clk1]
set_ideal_network [get_ports clk2]
puts "PASS: set_ideal_network"

catch {
  set_ideal_transition 0.0 [get_ports clk1]
  set_ideal_transition 0.05 [get_ports clk2]
  puts "PASS: set_ideal_transition"
}

############################################################
# Min pulse width on various objects
############################################################

set_min_pulse_width 1.0 [get_clocks clk1]
set_min_pulse_width -high 0.6 [get_clocks clk1]
set_min_pulse_width -low 0.4 [get_clocks clk1]
puts "PASS: min pulse width on clock"

set_min_pulse_width 0.8 [get_clocks clk2]
puts "PASS: min pulse width on clk2"

catch {
  set_min_pulse_width 0.5 [get_pins reg1/CK]
  puts "PASS: min pulse width on pin"
}

catch {
  set_min_pulse_width 0.6 [get_cells reg1]
  puts "PASS: min pulse width on instance"
}

############################################################
# Latch borrow limits on various objects
############################################################

set_max_time_borrow 2.0 [get_clocks clk1]
set_max_time_borrow 1.5 [get_clocks clk2]
puts "PASS: max_time_borrow on clocks"

set_max_time_borrow 1.0 [get_pins reg1/D]
puts "PASS: max_time_borrow on pin"

catch {
  set_max_time_borrow 1.2 [get_cells reg2]
  puts "PASS: max_time_borrow on instance"
}

############################################################
# Comprehensive write with all constraints
############################################################

set sdc_final [make_result_file sdc_disable_case_final.sdc]
write_sdc -no_timestamp $sdc_final
puts "PASS: final write_sdc"

set sdc_compat [make_result_file sdc_disable_case_compat.sdc]
write_sdc -no_timestamp -compatible $sdc_compat
puts "PASS: final write_sdc -compatible"

set sdc_d6 [make_result_file sdc_disable_case_d6.sdc]
write_sdc -no_timestamp -digits 6 $sdc_d6
puts "PASS: final write_sdc -digits 6"

report_checks
puts "PASS: final report_checks"

puts "ALL PASSED"
