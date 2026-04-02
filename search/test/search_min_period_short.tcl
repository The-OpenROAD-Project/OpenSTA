# Test CheckMinPeriods.cc and CheckMaxSkews.cc short report paths
# that are uncovered in the coverage report.
# Covers: ReportPath::reportCheck(MinPeriodCheck, false),
#         reportPeriodHeaderShort(), reportShort(MinPeriodCheck),
#         reportVerbose(MinPeriodCheck), MinPeriodCheck::slack/period/minPeriod,
#         MinPeriodSlackVisitor, MinPeriodViolatorsVisitor,
#         CheckMaxSkews accessor methods
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

# Very tight clock period to trigger min_period violations
create_clock -name fast_clk -period 0.05 [get_ports clk]
set_input_delay -clock fast_clk 0.01 [get_ports in1]
set_input_delay -clock fast_clk 0.01 [get_ports in2]
set_output_delay -clock fast_clk 0.01 [get_ports out1]

# Force timing computation
report_checks > /dev/null

puts "--- report_check_types -min_period (non-verbose = short) ---"
report_check_types -min_period

puts "--- report_check_types -min_period -verbose ---"
report_check_types -min_period -verbose

puts "--- report_check_types -min_period -violators ---"
report_check_types -min_period -violators

puts "--- report_check_types -min_period -violators -verbose ---"
report_check_types -min_period -violators -verbose

puts "--- min_period_check_slack short ---"
# TODO: sta::min_period_check_slack removed from SWIG interface
# set min_check [sta::min_period_check_slack]
# if { $min_check != "NULL" } {
#   sta::report_min_period_check $min_check 0
#   sta::report_min_period_check $min_check 1
# }
puts "min_period_check_slack: skipped (API removed)"
puts "--- min_period_violations short/verbose ---"
# TODO: sta::min_period_violations removed from SWIG interface
# set viols [sta::min_period_violations]
# puts "Min period violations count: [llength $viols]"
# if { [llength $viols] > 0 } {
#   sta::report_min_period_checks $viols 0
#   sta::report_min_period_checks $viols 1
# }
puts "min_period_violations: skipped (API removed)"

puts "--- max_skew_check_slack ---"
# TODO: sta::max_skew_check_slack removed from SWIG interface
# set max_skew_slack [sta::max_skew_check_slack]
# if { $max_skew_slack != "NULL" } {
#   sta::report_max_skew_check $max_skew_slack 0
#   sta::report_max_skew_check $max_skew_slack 1
#   puts "Max skew check reported"
# }
puts "max_skew_check_slack: skipped (API removed)"
puts "--- max_skew_violations ---"
# TODO: sta::max_skew_violations removed from SWIG interface
# set max_viols [sta::max_skew_violations]
# puts "Max skew violations count: [llength $max_viols]"
# if { [llength $max_viols] > 0 } {
#   sta::report_max_skew_checks $max_viols 0
#   sta::report_max_skew_checks $max_viols 1
# }
puts "max_skew_violations: skipped (API removed)"

puts "--- report_check_types -max_skew (short) ---"
report_check_types -max_skew

puts "--- report_check_types -max_skew -verbose ---"
report_check_types -max_skew -verbose

puts "--- report_check_types all with tight clock ---"
report_check_types -max_fanout -max_capacitance -max_slew -min_period -max_skew -min_pulse_width

report_check_types -max_fanout -max_capacitance -max_slew -min_period -max_skew -min_pulse_width -verbose

puts "--- report_check_types -violators ---"
report_check_types -max_fanout -max_capacitance -max_slew -min_period -max_skew -min_pulse_width -violators

puts "--- report_clock_min_period ---"
report_clock_min_period
report_clock_min_period -clocks fast_clk
report_clock_min_period -include_port_paths

puts "--- Now with normal clock period ---"
create_clock -name normal_clk -period 10 [get_ports clk]
report_checks > /dev/null

puts "--- report_check_types with normal clock ---"
report_check_types -min_period
report_check_types -min_period -verbose

puts "--- min_pulse_width short and verbose ---"
report_check_types -min_pulse_width
report_check_types -min_pulse_width -verbose
report_check_types -min_pulse_width
report_check_types -min_pulse_width -verbose
