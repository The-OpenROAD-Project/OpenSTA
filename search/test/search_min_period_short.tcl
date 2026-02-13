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
puts "PASS: min_period short"

puts "--- report_check_types -min_period -verbose ---"
report_check_types -min_period -verbose
puts "PASS: min_period verbose"

puts "--- report_check_types -min_period -violators ---"
report_check_types -min_period -violators
puts "PASS: min_period violators short"

puts "--- report_check_types -min_period -violators -verbose ---"
report_check_types -min_period -violators -verbose
puts "PASS: min_period violators verbose"

puts "--- min_period_check_slack short ---"
set min_check [sta::min_period_check_slack]
if { $min_check != "NULL" } {
  # Short report (verbose=0) triggers reportCheck(check, false)
  # which calls reportPeriodHeaderShort + reportShort
  sta::report_min_period_check $min_check 0
  puts "Min period short report done"
  # Verbose report (verbose=1)
  sta::report_min_period_check $min_check 1
  puts "Min period verbose report done"
}
puts "PASS: min_period_check_slack short/verbose"

puts "--- min_period_violations short/verbose ---"
set viols [sta::min_period_violations]
puts "Min period violations count: [llength $viols]"
if { [llength $viols] > 0 } {
  # Short report: triggers reportChecks(checks, false)
  sta::report_min_period_checks $viols 0
  puts "Violations short report done"
  # Verbose report: triggers reportChecks(checks, true)
  sta::report_min_period_checks $viols 1
  puts "Violations verbose report done"
}
puts "PASS: min_period_violations reports"

puts "--- max_skew_check_slack ---"
set max_skew_slack [sta::max_skew_check_slack]
if { $max_skew_slack != "NULL" } {
  sta::report_max_skew_check $max_skew_slack 0
  sta::report_max_skew_check $max_skew_slack 1
  puts "Max skew check reported"
} else {
  puts "No max skew check found (expected for Nangate45)"
}
puts "PASS: max_skew_check_slack"

puts "--- max_skew_violations ---"
catch {
  set max_viols [sta::max_skew_violations]
  puts "Max skew violations count: [llength $max_viols]"
  if { [llength $max_viols] > 0 } {
    sta::report_max_skew_checks $max_viols 0
    sta::report_max_skew_checks $max_viols 1
  }
}
puts "PASS: max_skew_violations"

puts "--- report_check_types -max_skew (short) ---"
report_check_types -max_skew
puts "PASS: max_skew short"

puts "--- report_check_types -max_skew -verbose ---"
report_check_types -max_skew -verbose
puts "PASS: max_skew verbose"

puts "--- report_check_types all with tight clock ---"
report_check_types -max_fanout -max_capacitance -max_slew -min_period -max_skew -min_pulse_width
puts "PASS: all check types short"

report_check_types -max_fanout -max_capacitance -max_slew -min_period -max_skew -min_pulse_width -verbose
puts "PASS: all check types verbose"

puts "--- report_check_types -violators ---"
report_check_types -max_fanout -max_capacitance -max_slew -min_period -max_skew -min_pulse_width -violators
puts "PASS: all check types violators"

puts "--- report_clock_min_period ---"
report_clock_min_period
report_clock_min_period -clocks fast_clk
report_clock_min_period -include_port_paths
puts "PASS: clock_min_period"

puts "--- Now with normal clock period ---"
create_clock -name normal_clk -period 10 [get_ports clk]
report_checks > /dev/null

puts "--- report_check_types with normal clock ---"
report_check_types -min_period
report_check_types -min_period -verbose
puts "PASS: min_period with normal clock"

puts "--- min_pulse_width short and verbose ---"
report_check_types -min_pulse_width
report_check_types -min_pulse_width -verbose
report_pulse_width_checks
report_pulse_width_checks -verbose
puts "PASS: pulse width checks"

puts "ALL PASSED"
