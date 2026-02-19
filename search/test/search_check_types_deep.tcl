# Test report_check_types with all individual flag combinations,
# report_tns/report_wns for min/max, worst_slack/total_negative_slack for
# min paths, min_slew/min_cap/min_fanout limit checks, recovery/removal
# check types, clock_gating_setup/hold, report_worst_slack.
# Targets: Search.tcl report_check_types (all branch combinations),
#          Sta.cc reportCheckType code paths,
#          Search.cc worstSlack for min, endpointViolation,
#          visitPathEnds with different path groups,
#          CheckMinPeriods, CheckMaxSkew
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_data_check_gated.v
link_design search_data_check_gated

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_input_delay -clock clk 0.5 [get_ports en]
set_input_delay -clock clk 0.5 [get_ports rst]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]
set_output_delay -clock clk 2.0 [get_ports out3]

# Set limits to generate violations
set_max_transition 0.001 [current_design]
set_max_capacitance 0.0001 [current_design]
set_max_fanout 1 [current_design]

# Force timing
report_checks -path_delay max > /dev/null
report_checks -path_delay min > /dev/null

############################################################
# report_check_types with no flags (default: all checks)
############################################################
puts "--- report_check_types (all defaults) ---"
report_check_types

puts "--- report_check_types -verbose ---"
report_check_types -verbose

puts "--- report_check_types -violators ---"
report_check_types -violators

############################################################
# Individual check type flags
############################################################
puts "--- report_check_types -max_delay ---"
report_check_types -max_delay

puts "--- report_check_types -min_delay ---"
report_check_types -min_delay

puts "--- report_check_types -max_delay -verbose ---"
report_check_types -max_delay -verbose

puts "--- report_check_types -min_delay -verbose ---"
report_check_types -min_delay -verbose

puts "--- report_check_types -max_delay -violators ---"
report_check_types -max_delay -violators

puts "--- report_check_types -min_delay -violators ---"
report_check_types -min_delay -violators

puts "--- report_check_types -max_delay -min_delay ---"
report_check_types -max_delay -min_delay

puts "--- report_check_types -max_delay -min_delay -verbose ---"
report_check_types -max_delay -min_delay -verbose

puts "--- report_check_types -max_delay -min_delay -violators ---"
report_check_types -max_delay -min_delay -violators

############################################################
# Recovery/Removal check types
############################################################
puts "--- report_check_types -recovery ---"
report_check_types -recovery

puts "--- report_check_types -removal ---"
report_check_types -removal

puts "--- report_check_types -recovery -verbose ---"
report_check_types -recovery -verbose

puts "--- report_check_types -removal -verbose ---"
report_check_types -removal -verbose

puts "--- report_check_types -recovery -violators ---"
report_check_types -recovery -violators

puts "--- report_check_types -removal -violators ---"
report_check_types -removal -violators

puts "--- report_check_types -recovery -removal ---"
report_check_types -recovery -removal

puts "--- report_check_types -recovery -removal -verbose ---"
report_check_types -recovery -removal -verbose

############################################################
# Clock gating check types
############################################################
puts "--- report_check_types -clock_gating_setup ---"
report_check_types -clock_gating_setup

puts "--- report_check_types -clock_gating_hold ---"
report_check_types -clock_gating_hold

puts "--- report_check_types -clock_gating_setup -verbose ---"
report_check_types -clock_gating_setup -verbose

puts "--- report_check_types -clock_gating_hold -verbose ---"
report_check_types -clock_gating_hold -verbose

puts "--- report_check_types -clock_gating_setup -clock_gating_hold ---"
report_check_types -clock_gating_setup -clock_gating_hold

############################################################
# Slew limits: max and min
############################################################
puts "--- report_check_types -max_slew ---"
report_check_types -max_slew

puts "--- report_check_types -max_slew -verbose ---"
report_check_types -max_slew -verbose

puts "--- report_check_types -max_slew -violators ---"
report_check_types -max_slew -violators

puts "--- report_check_types -min_slew ---"
report_check_types -min_slew

puts "--- report_check_types -min_slew -verbose ---"
report_check_types -min_slew -verbose

############################################################
# Capacitance limits: max and min
############################################################
puts "--- report_check_types -max_capacitance ---"
report_check_types -max_capacitance

puts "--- report_check_types -max_capacitance -verbose ---"
report_check_types -max_capacitance -verbose

puts "--- report_check_types -max_capacitance -violators ---"
report_check_types -max_capacitance -violators

puts "--- report_check_types -min_capacitance ---"
report_check_types -min_capacitance

puts "--- report_check_types -min_capacitance -verbose ---"
report_check_types -min_capacitance -verbose

############################################################
# Fanout limits: max and min
############################################################
puts "--- report_check_types -max_fanout ---"
report_check_types -max_fanout

puts "--- report_check_types -max_fanout -verbose ---"
report_check_types -max_fanout -verbose

puts "--- report_check_types -max_fanout -violators ---"
report_check_types -max_fanout -violators

puts "--- report_check_types -min_fanout ---"
report_check_types -min_fanout

puts "--- report_check_types -min_fanout -verbose ---"
report_check_types -min_fanout -verbose

############################################################
# Min pulse width and min period
############################################################
puts "--- report_check_types -min_pulse_width ---"
report_check_types -min_pulse_width

puts "--- report_check_types -min_pulse_width -verbose ---"
report_check_types -min_pulse_width -verbose

puts "--- report_check_types -min_pulse_width -violators ---"
report_check_types -min_pulse_width -violators

puts "--- report_check_types -min_period ---"
report_check_types -min_period

puts "--- report_check_types -min_period -verbose ---"
report_check_types -min_period -verbose

puts "--- report_check_types -min_period -violators ---"
report_check_types -min_period -violators

############################################################
# Max skew
############################################################
puts "--- report_check_types -max_skew ---"
report_check_types -max_skew

puts "--- report_check_types -max_skew -verbose ---"
report_check_types -max_skew -verbose

puts "--- report_check_types -max_skew -violators ---"
report_check_types -max_skew -violators

############################################################
# report_check_types with -no_line_splits
############################################################
puts "--- report_check_types -max_slew -verbose -no_line_splits ---"
report_check_types -max_slew -verbose -no_line_splits

############################################################
# report_worst_slack for min and max
############################################################
puts "--- report_worst_slack ---"
report_worst_slack -max
report_worst_slack -min
report_worst_slack -max -digits 6
report_worst_slack -min -digits 6

############################################################
# report_tns for min and max
############################################################
puts "--- report_tns ---"
report_tns -max
report_tns -min
report_tns -max -digits 6
report_tns -min -digits 6

############################################################
# report_wns for min and max
############################################################
puts "--- report_wns ---"
report_wns -max
report_wns -min
report_wns -max -digits 6
report_wns -min -digits 6

############################################################
# worst_slack and total_negative_slack direct calls
############################################################
puts "--- worst_slack direct ---"
set ws_max [worst_slack -max]
puts "worst_slack max: $ws_max"
set ws_min [worst_slack -min]
puts "worst_slack min: $ws_min"

puts "--- total_negative_slack direct ---"
set tns_max [total_negative_slack -max]
puts "tns max: $tns_max"
set tns_min [total_negative_slack -min]
puts "tns min: $tns_min"

puts "--- worst_negative_slack ---"
set wns_max [worst_negative_slack -max]
puts "worst_negative_slack max: $wns_max"
set wns_min [worst_negative_slack -min]
puts "worst_negative_slack min: $wns_min"

############################################################
# endpoint_violation_count
############################################################
puts "--- endpoint_violation_count ---"
puts "max violations: [sta::endpoint_violation_count max]"
puts "min violations: [sta::endpoint_violation_count min]"

############################################################
# report_pulse_width_checks and report_clock_min_period with verbosity
############################################################
puts "--- report_pulse_width_checks ---"
report_pulse_width_checks

puts "--- report_pulse_width_checks -verbose ---"
report_pulse_width_checks -verbose

puts "--- report_clock_min_period ---"
report_clock_min_period

puts "--- report_clock_min_period -include_port_paths ---"
report_clock_min_period -include_port_paths

############################################################
# report_clock_skew with various options
############################################################
puts "--- report_clock_skew ---"
report_clock_skew -setup
report_clock_skew -hold
report_clock_skew -setup -include_internal_latency
report_clock_skew -hold -include_internal_latency
report_clock_skew -setup -digits 6
report_clock_skew -hold -digits 6

############################################################
# Multiple limit checks in one report_check_types call
############################################################
puts "--- Combined check types ---"
report_check_types -max_slew -max_capacitance -max_fanout
report_check_types -max_slew -max_capacitance -max_fanout -verbose
report_check_types -max_slew -max_capacitance -max_fanout -violators

############################################################
# report_checks with -group_path_count and -endpoint_path_count
############################################################
puts "--- find_timing_paths with slack_max filtering ---"
set paths_neg [find_timing_paths -path_delay max -slack_max 0.0]
puts "Paths with slack <= 0: [llength $paths_neg]"
set paths_all [find_timing_paths -path_delay max -slack_max 100.0]
puts "Paths with slack <= 100: [llength $paths_all]"

puts "--- find_timing_paths min with slack filtering ---"
set paths_min_neg [find_timing_paths -path_delay min -slack_max 0.0]
puts "Min paths with slack <= 0: [llength $paths_min_neg]"
set paths_min_all [find_timing_paths -path_delay min -slack_max 100.0]
puts "Min paths with slack <= 100: [llength $paths_min_all]"
