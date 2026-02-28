# Test power analysis and activity reporting.
# Targets: Sta.cc power, powerPreamble, activity, ensureClkNetwork
#          Search.cc findAllArrivals, ClkNetwork.cc
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_multicorner_analysis.v
link_design search_multicorner_analysis

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.5 [get_ports in2]
set_input_delay -clock clk 0.8 [get_ports in3]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 1.5 [get_ports out2]

# Run timing first
report_checks -path_delay max > /dev/null

puts "--- report_power ---"
report_power

puts "--- report_power -instances ---"
report_power -instances [get_cells {reg1 reg2 and1 buf1}]

puts "--- report_power -digits 6 ---"
report_power -digits 6

puts "--- Pin activity ---"

puts "--- set_power_activity on pins ---"
set_power_activity -input_ports [get_ports in1] -activity 0.5 -duty 0.5
set_power_activity -input_ports [get_ports in2] -activity 0.3 -duty 0.4
set_power_activity -input_ports [get_ports in3] -activity 0.8 -duty 0.6
report_power

puts "--- set_power_activity on global ---"
set_power_activity -global -activity 0.2 -duty 0.5
report_power

puts "--- set_power_activity on input_ports ---"
set_power_activity -input -activity 0.4 -duty 0.5
report_power

puts "--- report_power with clock propagation ---"
set_propagated_clock [get_clocks clk]
report_power

puts "--- report with timing derate after power ---"
set_timing_derate -early 0.95
set_timing_derate -late 1.05
report_power
unset_timing_derate

puts "--- Slew limit checking after power ---"
set_max_transition 0.5 [current_design]
report_check_types -max_slew -verbose

puts "--- Capacitance limit checking ---"
set_max_capacitance 0.05 [current_design]
report_check_types -max_capacitance -verbose

puts "--- Fanout limit checking ---"
set_max_fanout 10 [current_design]
report_check_types -max_fanout -verbose

puts "--- Tight limits to create violations ---"
set_max_transition 0.001 [current_design]
report_check_types -max_slew -verbose
report_check_types -max_slew -violators

set_max_capacitance 0.0001 [current_design]
report_check_types -max_capacitance -verbose
report_check_types -max_capacitance -violators

set_max_fanout 1 [current_design]
report_check_types -max_fanout -verbose
report_check_types -max_fanout -violators
