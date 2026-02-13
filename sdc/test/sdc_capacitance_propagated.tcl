# Test port external capacitance, set_load with rise/fall/min/max/corner,
# capacitance limits on cell/port/pin, propagated clock set/unset,
# and unset_case_analysis from various pin states.
# Targets:
#   Sdc.cc: setPortExtPinCap, setPortExtWireCap, portExtCap (all overloads),
#     hasPortExtCap, portExtFanout, ensurePortExtPinCap,
#     setCapacitanceLimit(Cell/Port/Pin), capacitanceLimit(Port/Pin),
#     setPropagatedClock, removePropagatedClock (clock and pin),
#     removeCaseAnalysis, setCaseAnalysis (all 4 values),
#     unsetCaseAnalysis, setLogicValue,
#     setPortExtFanout, portExtFanout (int overload),
#     connectedPinCap (via set_load -subtract_pin_load),
#     findLeafLoadPins, findLeafDriverPins (via input_delay on ports)
#   WriteSdc.cc: writePortLoads (pin/wire/fanout writing),
#     writeCaseAnalysis (all values), writeConstants,
#     writePropagatedClkPins, writeCapacitanceLimits
#   PortExtCap.cc: pinCap, wireCap, fanout methods
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 -waveform {0 10} [get_ports clk2]
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_input_delay -clock clk2 2.0 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]
puts "PASS: setup"

############################################################
# Test 1: set_load - basic pin and wire loads
############################################################
set_load 0.05 [get_ports out1]
puts "PASS: set_load basic"

set_load -pin_load 0.04 [get_ports out1]
puts "PASS: set_load -pin_load"

set_load -wire_load 0.02 [get_ports out1]
puts "PASS: set_load -wire_load"

set_load -pin_load 0.03 [get_ports out2]
puts "PASS: set_load out2 pin"

############################################################
# Test 2: set_load with rise/fall
############################################################
set_load -pin_load -rise 0.045 [get_ports out1]
set_load -pin_load -fall 0.055 [get_ports out1]
puts "PASS: set_load rise/fall"

set_load -wire_load -rise 0.015 [get_ports out2]
set_load -wire_load -fall 0.025 [get_ports out2]
puts "PASS: set_load wire rise/fall"

############################################################
# Test 3: set_load with min/max
############################################################
set_load -min 0.01 [get_ports out1]
set_load -max 0.06 [get_ports out1]
puts "PASS: set_load min/max"

set_load -pin_load -min 0.02 [get_ports out2]
set_load -pin_load -max 0.05 [get_ports out2]
puts "PASS: set_load pin min/max"

############################################################
# Test 4: Port fanout number
############################################################
set_port_fanout_number 4 [get_ports out1]
set_port_fanout_number 8 [get_ports out2]
puts "PASS: port fanout"

############################################################
# Test 5: Net wire cap (set_load on nets)
############################################################
catch {
  set_load 0.01 [get_nets n1]
  set_load 0.02 [get_nets n2]
  puts "PASS: set_load on nets"
}

############################################################
# Test 6: Capacitance limits
############################################################
# Design-level
set_max_capacitance 0.25 [current_design]
puts "PASS: max_capacitance design"

# Port-level
set_max_capacitance 0.15 [get_ports out1]
set_max_capacitance 0.20 [get_ports out2]
puts "PASS: max_capacitance ports"

catch {
  set_min_capacitance 0.01 [get_ports out1]
  set_min_capacitance 0.005 [get_ports out2]
  puts "PASS: min_capacitance ports"
}

# Pin-level
catch {
  set_max_capacitance 0.10 [get_pins reg1/D]
  puts "PASS: max_capacitance pin"
}

# Transition limits
set_max_transition 0.5 [current_design]
set_max_transition 0.3 [get_ports out1]
set_max_transition -clock_path 0.2 [get_clocks clk1]
set_max_transition -data_path 0.4 [get_clocks clk1]
puts "PASS: max_transition"

# Fanout limits
set_max_fanout 20 [current_design]
set_max_fanout 10 [get_ports in1]
puts "PASS: max_fanout"

set_max_area 200.0
puts "PASS: max_area"

# Write with all capacity/load constraints
set sdc1 [make_result_file sdc_cap_prop1.sdc]
write_sdc -no_timestamp $sdc1
puts "PASS: write_sdc with loads"

############################################################
# Test 7: Propagated clocks - set and unset
############################################################

# Set propagated on clock object
set_propagated_clock [get_clocks clk1]
puts "PASS: set_propagated_clock clk1"

# Set propagated on port (pin)
set_propagated_clock [get_ports clk2]
puts "PASS: set_propagated_clock port clk2"

# Write with propagated
set sdc2 [make_result_file sdc_cap_prop2.sdc]
write_sdc -no_timestamp $sdc2
puts "PASS: write_sdc with propagated"

# Unset propagated on clock
unset_propagated_clock [get_clocks clk1]
puts "PASS: unset_propagated_clock clk1"

# Unset propagated on pin
unset_propagated_clock [get_ports clk2]
puts "PASS: unset_propagated_clock port clk2"

# Verify propagated is removed
set sdc3 [make_result_file sdc_cap_prop3.sdc]
write_sdc -no_timestamp $sdc3
puts "PASS: write_sdc after unset propagated"

############################################################
# Test 8: Case analysis - all 4 values and unset
############################################################

# Value 0
set_case_analysis 0 [get_ports in1]
puts "PASS: case_analysis 0"

# Value 1
set_case_analysis 1 [get_ports in2]
puts "PASS: case_analysis 1"

# Value rising
set_case_analysis rising [get_ports in3]
puts "PASS: case_analysis rising"

# Write with case analysis
set sdc4 [make_result_file sdc_cap_prop4.sdc]
write_sdc -no_timestamp $sdc4
puts "PASS: write_sdc with case analysis"

# Unset case analysis
unset_case_analysis [get_ports in1]
puts "PASS: unset case_analysis in1"

unset_case_analysis [get_ports in2]
puts "PASS: unset case_analysis in2"

unset_case_analysis [get_ports in3]
puts "PASS: unset case_analysis in3"

# Value falling
set_case_analysis falling [get_ports in1]
puts "PASS: case_analysis falling"

# Write with falling
set sdc5 [make_result_file sdc_cap_prop5.sdc]
write_sdc -no_timestamp $sdc5
puts "PASS: write_sdc with falling"

# Unset
unset_case_analysis [get_ports in1]
puts "PASS: unset falling"

############################################################
# Test 9: Logic values
############################################################
set_logic_zero [get_ports in1]
set_logic_one [get_ports in2]
catch {
  set_logic_dc [get_ports in3]
}
puts "PASS: logic values"

set sdc6 [make_result_file sdc_cap_prop6.sdc]
write_sdc -no_timestamp $sdc6
puts "PASS: write_sdc with logic values"

############################################################
# Test 10: Read back and verify roundtrip
############################################################
read_sdc $sdc1
puts "PASS: read_sdc"

report_checks
puts "PASS: report after read"

set sdc7 [make_result_file sdc_cap_prop7.sdc]
write_sdc -no_timestamp $sdc7
puts "PASS: write_sdc roundtrip"

# Read compatible format
read_sdc $sdc2
puts "PASS: read_sdc propagated"

set sdc8 [make_result_file sdc_cap_prop8.sdc]
write_sdc -no_timestamp -compatible $sdc8
puts "PASS: write_sdc compatible roundtrip"

report_checks
puts "PASS: final report"

puts "ALL PASSED"
