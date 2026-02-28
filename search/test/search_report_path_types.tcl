# Test ReportPath.cc and PathEnd.cc: various report formats, path types,
# and check types that exercise reporting code in different branches.
# Targets: ReportPath.cc reportPathEnd, reportPathHeader,
#   reportPath (full, full_clock, full_clock_expanded, end, summary),
#   reportShort/Verbose for setup/hold/recovery/removal,
#   reportCheck for MinPeriod/MaxSkew/MinPulseWidth,
#   PathEnd.cc PathEndClkConstrained, PathEndCheck,
#   pathEndLess, pathEndEqual,
#   Search.cc reportPathEnd, visitPathEnds with different check types
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_path_end_types.v
link_design search_path_end_types

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_input_delay -clock clk 0.5 [get_ports rst]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]
set_output_delay -clock clk 2.0 [get_ports out3]
set_input_transition 0.1 [all_inputs]

report_checks > /dev/null

############################################################
# report_checks with all format options
############################################################
puts "--- report_checks -format full ---"
report_checks -path_delay max -format full

puts "--- report_checks -format full_clock ---"
report_checks -path_delay max -format full_clock

puts "--- report_checks -format full_clock_expanded ---"
report_checks -path_delay max -format full_clock_expanded

puts "--- report_checks -format end ---"
report_checks -path_delay max -format end

puts "--- report_checks -format summary ---"
report_checks -path_delay max -format summary

puts "--- report_checks min -format full ---"
report_checks -path_delay min -format full

puts "--- report_checks min -format full_clock ---"
report_checks -path_delay min -format full_clock

puts "--- report_checks min -format full_clock_expanded ---"
report_checks -path_delay min -format full_clock_expanded

puts "--- report_checks min -format end ---"
report_checks -path_delay min -format end

puts "--- report_checks min -format summary ---"
report_checks -path_delay min -format summary

############################################################
# report_checks with -fields combinations
############################################################
puts "--- report_checks -fields slew ---"
report_checks -path_delay max -fields {slew}

puts "--- report_checks -fields cap ---"
report_checks -path_delay max -fields {cap}

puts "--- report_checks -fields input_pins ---"
report_checks -path_delay max -fields {input_pins}

puts "--- report_checks -fields nets ---"
report_checks -path_delay max -fields {nets}

puts "--- report_checks -fields fanout ---"
report_checks -path_delay max -fields {fanout}

puts "--- report_checks -fields all ---"
report_checks -path_delay max -fields {slew cap input_pins nets fanout}

puts "--- report_checks min -fields all ---"
report_checks -path_delay min -fields {slew cap input_pins nets fanout}

############################################################
# report_checks with -digits
############################################################
puts "--- report_checks -digits 2 ---"
report_checks -path_delay max -digits 2

puts "--- report_checks -digits 6 ---"
report_checks -path_delay max -digits 6

############################################################
# Recovery/removal check reporting (async reset paths)
############################################################
puts "--- report_checks recovery ---"
report_check_types -recovery

puts "--- report_checks removal ---"
report_check_types -removal

puts "--- report_checks recovery -verbose ---"
report_check_types -recovery -verbose

puts "--- report_checks removal -verbose ---"
report_check_types -removal -verbose

############################################################
# Min period and pulse width checks
############################################################
puts "--- report_check_types -min_period ---"
report_check_types -min_period

puts "--- report_check_types -min_period -verbose ---"
report_check_types -min_period -verbose

puts "--- report_check_types -min_pulse_width ---"
report_check_types -min_pulse_width

puts "--- report_check_types -min_pulse_width -verbose ---"
report_check_types -min_pulse_width -verbose

############################################################
# Slew/fanout/cap checks
############################################################
puts "--- report_check_types -max_slew ---"
report_check_types -max_slew

puts "--- report_check_types -max_slew -verbose ---"
report_check_types -max_slew -verbose

puts "--- report_check_types -max_capacitance ---"
report_check_types -max_capacitance

puts "--- report_check_types -max_capacitance -verbose ---"
report_check_types -max_capacitance -verbose

puts "--- report_check_types -max_fanout ---"
report_check_types -max_fanout

puts "--- report_check_types -max_fanout -verbose ---"
report_check_types -max_fanout -verbose

############################################################
# Max skew checks
############################################################
puts "--- report_check_types -max_skew ---"
report_check_types -max_skew

puts "--- report_check_types -max_skew -verbose ---"
report_check_types -max_skew -verbose

############################################################
# Violators summary
############################################################
puts "--- report_check_types -violators ---"
report_check_types -violators

puts "--- report_check_types -violators -verbose ---"
report_check_types -violators -verbose

############################################################
# Clock gating checks
############################################################
puts "--- report_check_types -clock_gating_setup ---"
report_check_types -clock_gating_setup

puts "--- report_check_types -clock_gating_hold ---"
report_check_types -clock_gating_hold

puts "--- report_check_types both ---"
report_check_types -clock_gating_setup -clock_gating_hold

############################################################
# report_checks -unconstrained (exercises unconstrained path reporting)
############################################################
puts "--- report_checks -unconstrained ---"
report_checks -unconstrained

############################################################
# report_checks with JSON format
############################################################
puts "--- report_checks -format json ---"
report_checks -path_delay max -format json
