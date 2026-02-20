# Test multi-corner analysis, operating conditions, voltage,
# analysis type changes, port external caps, wire caps.
# Targets: Sta.cc setAnalysisType, setVoltage, setOperatingConditions,
#          setPortExtPinCap, setPortExtWireCap, setPortExtFanout, setNetWireCap,
#          connectedCap, portExtCaps, removeNetLoadCaps, setTimingDerate variants
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

# Force initial timing
report_checks > /dev/null

puts "--- set_analysis_type bc_wc ---"
set_operating_conditions -analysis_type bc_wc
report_checks -path_delay max > /dev/null

puts "--- set_analysis_type single ---"
set_operating_conditions -analysis_type single
report_checks -path_delay max > /dev/null

puts "--- set_analysis_type on_chip_variation ---"
set_operating_conditions -analysis_type on_chip_variation
report_checks -path_delay max -format full_clock_expanded
report_checks -path_delay min -format full_clock_expanded

puts "--- set_voltage ---"
set_voltage 1.1
set_voltage 1.1 -min 0.9
report_checks -path_delay max > /dev/null

puts "--- set_voltage on net ---"
set_voltage 1.2 -object_list [get_nets n1]
set_voltage 1.2 -min 1.0 -object_list [get_nets n1]
report_checks -path_delay max > /dev/null

puts "--- set_load (port external pin cap) ---"
set_load 0.05 [get_ports out1]
set_load 0.03 [get_ports out2]
report_checks -path_delay max

puts "--- set_load with -min/-max ---"
set_load -min 0.02 [get_ports out1]
set_load -max 0.08 [get_ports out1]
report_checks -path_delay max
report_checks -path_delay min

puts "--- set_load -wire_load ---"
set_load -wire_load 0.01 [get_ports out1]
report_checks -path_delay max > /dev/null

puts "--- set_fanout_load ---"
set_fanout_load 4 [get_ports out1]
report_checks -path_delay max > /dev/null

puts "--- Net capacitance ---"
set corner [sta::cmd_corner]
set net_cap [[get_nets n1] capacitance $corner max]
puts "Net n1 capacitance: $net_cap"
set pin_cap [[get_nets n1] pin_capacitance $corner max]
puts "Net n1 pin_cap: $pin_cap"
set wire_cap [[get_nets n1] wire_capacitance $corner max]
puts "Net n1 wire_cap: $wire_cap"

puts "--- set_wire_load_mode ---"
set_wire_load_mode enclosed

puts "--- report_checks with various fields after load changes ---"
report_checks -fields {capacitance slew fanout} -path_delay max
report_checks -fields {capacitance slew fanout} -path_delay min

puts "--- set_input_transition ---"
set_input_transition 0.5 [get_ports in1]
set_input_transition 0.3 [get_ports in2]
set_input_transition 0.4 [get_ports in3]
report_checks -path_delay max -fields {slew}
report_checks -path_delay min -fields {slew}

puts "--- set_drive on port ---"
set_drive 100 [get_ports in1]
report_checks -path_delay max > /dev/null

puts "--- set_driving_cell ---"
set_driving_cell -lib_cell BUF_X1 -library NangateOpenCellLibrary [get_ports in1]
report_checks -path_delay max -from [get_ports in1]

puts "--- Timing derate with cell-level ---"
set_timing_derate -early 0.95
set_timing_derate -late 1.05
report_checks -path_delay max > /dev/null
set_timing_derate -early -cell_delay 0.93
set_timing_derate -late -cell_delay 1.07
report_checks -path_delay max > /dev/null
set_timing_derate -early -net_delay 0.96
set_timing_derate -late -net_delay 1.04
report_checks -path_delay max > /dev/null
unset_timing_derate

puts "--- report_checks after all modifications ---"
report_checks -path_delay max -format full_clock_expanded
report_checks -path_delay min -format full_clock_expanded
report_checks -path_delay max -format json

puts "--- report_check_types verbose after modifications ---"
report_check_types -verbose

puts "--- write_sdc ---"
set sdc_file [make_result_file "search_multicorner_analysis.sdc"]
write_sdc $sdc_file

puts "--- set_resistance on net ---"
set_resistance 100 [get_nets n1]
report_checks -path_delay max > /dev/null

puts "--- set_max_area ---"
set_max_area 1000

puts "--- isClock / isPropagatedClock queries ---"
catch {
  set clk_pin [get_pins ckbuf/Z]
  puts "isClock ckbuf/Z: [sta::is_clock_pin $clk_pin]"
}
