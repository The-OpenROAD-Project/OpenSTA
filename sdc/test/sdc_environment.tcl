# Test design environment SDC commands for code coverage
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

# Setup clock
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_input_delay -clock clk1 2.0 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]
puts "PASS: basic setup"

############################################################
# set_driving_cell
############################################################

set_driving_cell -lib_cell BUF_X1 [get_ports in1]
puts "PASS: set_driving_cell BUF_X1"

set_driving_cell -lib_cell INV_X1 [get_ports in2]
puts "PASS: set_driving_cell INV_X1"

set_driving_cell -lib_cell BUF_X4 [get_ports in3]
puts "PASS: set_driving_cell BUF_X4"

report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: report_checks after set_driving_cell"

############################################################
# set_load
############################################################

set_load 0.05 [get_ports out1]
puts "PASS: set_load"

set_load -pin_load 0.03 [get_ports out2]
puts "PASS: set_load -pin_load"

report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: report_checks after set_load"

############################################################
# set_input_transition
############################################################

set_input_transition 0.15 [get_ports in1]
puts "PASS: set_input_transition"

set_input_transition -rise 0.12 [get_ports in2]
puts "PASS: set_input_transition -rise"

set_input_transition -fall 0.18 [get_ports in2]
puts "PASS: set_input_transition -fall"

set_input_transition -min 0.08 [get_ports in3]
puts "PASS: set_input_transition -min"

set_input_transition -max 0.20 [get_ports in3]
puts "PASS: set_input_transition -max"

report_checks -from [get_ports in2] -to [get_ports out1]
puts "PASS: report_checks after set_input_transition"

############################################################
# set_max_capacitance / set_max_transition / set_max_fanout
############################################################

set_max_capacitance 0.1 [get_ports out1]
puts "PASS: set_max_capacitance port"

set_max_capacitance 0.2 [current_design]
puts "PASS: set_max_capacitance design"

set_max_transition 0.5 [get_ports out1]
puts "PASS: set_max_transition port"

set_max_transition 1.0 [current_design]
puts "PASS: set_max_transition design"

set_max_transition -clock_path 0.3 [get_clocks clk1]
puts "PASS: set_max_transition -clock_path"

set_max_transition -data_path 0.8 [get_clocks clk1]
puts "PASS: set_max_transition -data_path"

set_max_fanout 10 [get_ports in1]
puts "PASS: set_max_fanout port"

set_max_fanout 20 [current_design]
puts "PASS: set_max_fanout design"

############################################################
# set_case_analysis
############################################################

set_case_analysis 0 [get_ports in3]
puts "PASS: set_case_analysis 0"

report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: report_checks after case_analysis 0"

unset_case_analysis [get_ports in3]
puts "PASS: unset_case_analysis"

set_case_analysis 1 [get_ports in3]
puts "PASS: set_case_analysis 1"

report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: report_checks after case_analysis 1"

unset_case_analysis [get_ports in3]
puts "PASS: unset_case_analysis second"

############################################################
# set_logic_zero / set_logic_one / set_logic_dc
############################################################

set_logic_zero [get_ports in3]
puts "PASS: set_logic_zero"

set_logic_one [get_ports in2]
puts "PASS: set_logic_one"

set_logic_dc [get_ports in1]
puts "PASS: set_logic_dc"

report_checks
puts "PASS: report_checks after logic values"

############################################################
# set_ideal_network
############################################################

set_ideal_network [get_ports clk1]
puts "PASS: set_ideal_network"

############################################################
# set_operating_conditions
############################################################

set_operating_conditions typical
puts "PASS: set_operating_conditions"

############################################################
# set_wire_load_model
############################################################

set_wire_load_model -name "5K_hvratio_1_1"
puts "PASS: set_wire_load_model"

set_wire_load_mode enclosed
puts "PASS: set_wire_load_mode enclosed"

############################################################
# set_timing_derate
############################################################

set_timing_derate -early 0.95
puts "PASS: set_timing_derate -early"

set_timing_derate -late 1.05
puts "PASS: set_timing_derate -late"

report_checks
puts "PASS: report_checks after timing derate"

unset_timing_derate
puts "PASS: unset_timing_derate"

############################################################
# set_max_area
############################################################

set_max_area 100.0
puts "PASS: set_max_area"

############################################################
# set_disable_timing
############################################################

# Disable timing on an instance
set_disable_timing [get_cells buf1]
puts "PASS: set_disable_timing instance"

unset_disable_timing [get_cells buf1]
puts "PASS: unset_disable_timing instance"

# Disable timing on a pin
set_disable_timing [get_pins buf1/A]
puts "PASS: set_disable_timing pin"

unset_disable_timing [get_pins buf1/A]
puts "PASS: unset_disable_timing pin"

############################################################
# set_min_pulse_width
############################################################

set_min_pulse_width 1.0 [get_clocks clk1]
puts "PASS: set_min_pulse_width"

############################################################
# Port external capacitance / fanout
############################################################

set_port_fanout_number 4 [get_ports out1]
puts "PASS: set_port_fanout_number"

set_resistance -min 10.0 [get_nets n1]
puts "PASS: set_resistance -min"

set_resistance -max 20.0 [get_nets n1]
puts "PASS: set_resistance -max"

############################################################
# set_voltage
############################################################

catch {set_voltage 1.1 -min 0.9}
puts "PASS: set_voltage"

############################################################
# Final report
############################################################

report_checks
puts "PASS: final report_checks"

report_check_types -max_capacitance -max_slew -max_fanout
puts "PASS: report_check_types"

puts "ALL PASSED"
