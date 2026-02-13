# Test comprehensive power analysis
# Exercises: set_power_activity, report_power with various options, unset_power_activity

read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog power_test1.v
link_design power_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_output_delay -clock clk 0 [get_ports out1]

#---------------------------------------------------------------
# Basic power report
#---------------------------------------------------------------
puts "--- Basic power report ---"
report_power
puts "PASS: basic report_power"

#---------------------------------------------------------------
# Set global activity and report
#---------------------------------------------------------------
puts "--- Power with global activity ---"
set_power_activity -global -activity 0.5 -duty 0.5
report_power
puts "PASS: report_power with global activity"

#---------------------------------------------------------------
# report_power -digits
#---------------------------------------------------------------
puts "--- Power with -digits 6 ---"
report_power -digits 6
puts "PASS: report_power -digits 6"

report_power -digits 3
puts "PASS: report_power -digits 3"

#---------------------------------------------------------------
# report_power -instances
#---------------------------------------------------------------
puts "--- Power for specific instances ---"
report_power -instances [get_cells buf1]
puts "PASS: report_power -instances buf1"

report_power -instances [get_cells reg1]
puts "PASS: report_power -instances reg1"

report_power -instances [get_cells *]
puts "PASS: report_power -instances all cells"

#---------------------------------------------------------------
# report_power -instances with -digits
#---------------------------------------------------------------
puts "--- Instance power with -digits ---"
report_power -instances [get_cells *] -digits 6
puts "PASS: report_power -instances -digits 6"

#---------------------------------------------------------------
# Set pin-specific activity and report
#---------------------------------------------------------------
puts "--- Power with pin activity ---"
set_power_activity -pins [get_pins reg1/CLK] -activity 1.0 -duty 0.5
report_power
puts "PASS: report_power with pin activity"

#---------------------------------------------------------------
# Set input activity
#---------------------------------------------------------------
puts "--- Power with input activity ---"
set_power_activity -input -activity 0.3 -duty 0.5
report_power
puts "PASS: report_power with input activity"

#---------------------------------------------------------------
# Set input port-specific activity
#---------------------------------------------------------------
puts "--- Power with input_port activity ---"
set_power_activity -input_ports [get_ports in1] -activity 0.2 -duty 0.5
report_power
puts "PASS: report_power with input_port activity"

#---------------------------------------------------------------
# Unset activities
#---------------------------------------------------------------
puts "--- Unset activities ---"
unset_power_activity -global
report_power
puts "PASS: report_power after unset_power_activity -global"

unset_power_activity -input
report_power
puts "PASS: report_power after unset_power_activity -input"

unset_power_activity -input_ports [get_ports in1]
report_power
puts "PASS: report_power after unset_power_activity -input_ports"

unset_power_activity -pins [get_pins reg1/CLK]
report_power
puts "PASS: report_power after unset_power_activity -pins"

#---------------------------------------------------------------
# Power with density instead of activity
#---------------------------------------------------------------
puts "--- Power with density ---"
set_power_activity -global -density 1e8 -duty 0.5
report_power
puts "PASS: report_power with density"

unset_power_activity -global
puts "PASS: unset after density test"

#---------------------------------------------------------------
# report_power -format json
#---------------------------------------------------------------
puts "--- Power JSON format ---"
set_power_activity -global -activity 0.5 -duty 0.5
report_power -format json
puts "PASS: report_power -format json"

report_power -instances [get_cells *] -format json
puts "PASS: report_power -instances -format json"

puts "ALL PASSED"
