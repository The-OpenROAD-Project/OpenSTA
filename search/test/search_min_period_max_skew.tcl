# Test CheckMinPeriods.cc, CheckMaxSkews.cc
# Exercises min_period_violations, min_period_check_slack,
# max_skew_violations, max_skew_check_slack
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

# Create a very tight clock to trigger min period violations
create_clock -name fast_clk -period 0.05 [get_ports clk]
set_input_delay -clock fast_clk 0.01 [get_ports in1]
set_input_delay -clock fast_clk 0.01 [get_ports in2]
set_output_delay -clock fast_clk 0.01 [get_ports out1]

puts "--- report_check_types -min_period with tight clock ---"
report_check_types -min_period -verbose

puts "--- report_clock_min_period with tight clock ---"
report_clock_min_period
report_clock_min_period -include_port_paths
report_clock_min_period -clocks fast_clk

puts "--- min_period_violations ---"
set min_period_viols [sta::min_period_violations]
puts "Min period violations: [llength $min_period_viols]"

puts "--- min_period_check_slack ---"
set min_period_slack_check [sta::min_period_check_slack]
if { $min_period_slack_check != "NULL" } {
  sta::report_min_period_check $min_period_slack_check 1
  puts "Min period slack check reported"
}

puts "--- report_min_period_checks ---"
set checks [sta::min_period_violations]
sta::report_min_period_checks $checks 0
sta::report_min_period_checks $checks 1

puts "--- max_skew checks ---"
# Add max_skew constraint via set_max_skew (if available) or
# exercise the code through report_check_types
report_check_types -max_skew -verbose

puts "--- max_skew_violations ---"
set max_skew_viols [sta::max_skew_violations]
puts "Max skew violations: [llength $max_skew_viols]"
sta::report_max_skew_checks $max_skew_viols 0
sta::report_max_skew_checks $max_skew_viols 1

puts "--- max_skew_check_slack ---"
set max_skew_slack [sta::max_skew_check_slack]
if { $max_skew_slack != "NULL" } {
  sta::report_max_skew_check $max_skew_slack 0
  sta::report_max_skew_check $max_skew_slack 1
  puts "Max skew slack check reported"
}

puts "--- Now with normal clock period ---"
# Recreate clock with normal period
create_clock -name fast_clk -period 10 [get_ports clk]

report_check_types -min_period -verbose
report_check_types -max_skew -verbose

puts "--- min_pulse_width checks ---"
report_check_types -min_pulse_width -verbose
report_pulse_width_checks
report_pulse_width_checks -verbose
