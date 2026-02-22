# Test MakeTimingModel.cc deeper: write_timing_model with various options,
# then read back the generated liberty. Also test min_period, max_skew checks,
# and clock latency/skew report functions.
# Targets: MakeTimingModel.cc makeTimingModel, makeGateModelScalar,
#          checkClock, OutputDelays, MakeEndTimingArcs,
#          Sta.cc writeTimingModel, reportCheck(MinPeriod/MaxSkew),
#          ReportPath.cc reportCheck(MinPeriodCheck), reportCheck(MaxSkewCheck),
#          reportPeriodHeaderShort, reportMaxSkewHeaderShort,
#          reportShort/Verbose for MinPeriod and MaxSkew,
#          ClkSkew.cc ClkSkew copy, findClkSkew
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

# Run timing first
report_checks -path_delay max > /dev/null

############################################################
# write_timing_model with various options
############################################################
puts "--- write_timing_model default ---"
set model_file1 [make_result_file "timing_model_deep1.lib"]
write_timing_model $model_file1

puts "--- write_timing_model -cell_name ---"
set model_file2 [make_result_file "timing_model_deep2.lib"]
write_timing_model -cell_name custom_cell $model_file2

puts "--- write_timing_model -library_name -cell_name ---"
set model_file3 [make_result_file "timing_model_deep3.lib"]
write_timing_model -library_name custom_lib -cell_name custom_cell $model_file3

puts "--- write_timing_model -corner ---"
set corner [sta::cmd_corner]
set model_file4 [make_result_file "timing_model_deep4.lib"]
write_timing_model -corner [$corner name] $model_file4

############################################################
# Read back the generated timing model
############################################################
puts "--- Read back generated model ---"
read_liberty $model_file1

############################################################
# Min period checks
############################################################
puts "--- min_period_violations ---"
set mpv [sta::min_period_violations]
puts "min_period violations: [llength $mpv]"

puts "--- min_period_check_slack ---"
set mpc [sta::min_period_check_slack]
if { $mpc != "NULL" } {
  sta::report_min_period_check $mpc 0
  sta::report_min_period_check $mpc 1
}

puts "--- report_min_period_checks ---"
set mpc_all [sta::min_period_violations]
sta::report_min_period_checks $mpc_all 0
sta::report_min_period_checks $mpc_all 1

############################################################
# Max skew checks
############################################################
puts "--- max_skew_violations ---"
set msv [sta::max_skew_violations]
puts "max_skew violations: [llength $msv]"

puts "--- max_skew_check_slack ---"
set msc [sta::max_skew_check_slack]
if { $msc != "NULL" } {
  sta::report_max_skew_check $msc 0
  sta::report_max_skew_check $msc 1
}

############################################################
# Clock skew with various options
############################################################
puts "--- report_clock_skew -setup ---"
report_clock_skew -setup

puts "--- report_clock_skew -hold ---"
report_clock_skew -hold

puts "--- report_clock_skew -digits 6 ---"
report_clock_skew -setup -digits 6

puts "--- report_clock_skew -clock clk ---"
report_clock_skew -setup -clock clk

puts "--- report_clock_skew -include_internal_latency ---"
report_clock_skew -setup -include_internal_latency

############################################################
# Clock latency
############################################################
puts "--- report_clock_latency ---"
report_clock_latency

puts "--- report_clock_latency -include_internal_latency ---"
report_clock_latency -include_internal_latency

puts "--- report_clock_latency -clock clk ---"
report_clock_latency -clock clk

puts "--- report_clock_latency -digits 6 ---"
report_clock_latency -digits 6

############################################################
# Clock min period
############################################################
puts "--- report_clock_min_period ---"
report_clock_min_period

puts "--- report_clock_min_period -include_port_paths ---"
report_clock_min_period -include_port_paths

puts "--- report_clock_min_period -clocks ---"
report_clock_min_period -clocks clk

############################################################
# find_clk_min_period
############################################################
puts "--- find_clk_min_period ---"
set clk_obj [lindex [all_clocks] 0]
set mp [sta::find_clk_min_period $clk_obj 0]
puts "clk min_period: $mp"
set mp2 [sta::find_clk_min_period $clk_obj 1]
puts "clk min_period (with port): $mp2"

############################################################
# Pulse width checks
############################################################
puts "--- report_pulse_width_checks ---"
report_pulse_width_checks

puts "--- report_pulse_width_checks -verbose ---"
report_pulse_width_checks -verbose

puts "--- min_pulse_width_checks ---"
set mpwc [sta::min_pulse_width_checks "NULL"]
puts "mpw checks: [llength $mpwc]"

puts "--- min_pulse_width_violations ---"
set mpwv [sta::min_pulse_width_violations "NULL"]
puts "mpw violations: [llength $mpwv]"

puts "--- min_pulse_width_check_slack ---"
set mpws [sta::min_pulse_width_check_slack "NULL"]
if { $mpws != "NULL" } {
  sta::report_mpw_check $mpws 0
  sta::report_mpw_check $mpws 1
}

############################################################
# Violation counts
############################################################
puts "--- max_slew_violation_count ---"
set slew_vc [sta::max_slew_violation_count]
puts "max slew violations: $slew_vc"

puts "--- max_fanout_violation_count ---"
set fan_vc [sta::max_fanout_violation_count]
puts "max fanout violations: $fan_vc"

puts "--- max_capacitance_violation_count ---"
set cap_vc [sta::max_capacitance_violation_count]
puts "max cap violations: $cap_vc"

############################################################
# Slew/fanout/cap check slack and limit
############################################################
puts "--- max_slew_check_slack ---"
set ss [sta::max_slew_check_slack]
set sl [sta::max_slew_check_limit]
puts "max slew slack: $ss limit: $sl"

puts "--- max_fanout_check_slack ---"
set fs [sta::max_fanout_check_slack]
set fl [sta::max_fanout_check_limit]
puts "max fanout slack: $fs limit: $fl"

puts "--- max_capacitance_check_slack ---"
set cs [sta::max_capacitance_check_slack]
set cl [sta::max_capacitance_check_limit]
puts "max cap slack: $cs limit: $cl"
