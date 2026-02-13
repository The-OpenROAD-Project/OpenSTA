# Test ClkSkew.cc with propagated clock: report_clock_skew with various
# combinations of setup/hold, digits, named clocks, and internal latency.
# Uses the CRPR design which has a real clock tree for propagated clock.
# Targets: ClkSkew.cc ClkSkew constructor, srcLatency, tgtLatency,
#   crpr, uncertainty, skew, findClkSkew,
#   reportClkSkew with different options,
#   Sta.cc reportClockSkew, reportClockLatency, reportClockMinPeriod
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_crpr.v
link_design search_crpr

############################################################
# Setup clock with propagation (clock tree buffers)
############################################################
create_clock -name clk -period 10 [get_ports clk]
set_propagated_clock [get_clocks clk]

set_input_delay -clock clk 0.5 [get_ports in1]
set_input_delay -clock clk 0.5 [get_ports in2]
set_output_delay -clock clk 1.0 [get_ports out1]
set_input_transition 0.1 [all_inputs]

report_checks > /dev/null

############################################################
# report_clock_skew with various options
############################################################
puts "--- report_clock_skew -setup ---"
report_clock_skew -setup
puts "PASS: clock_skew setup"

puts "--- report_clock_skew -hold ---"
report_clock_skew -hold
puts "PASS: clock_skew hold"

puts "--- report_clock_skew -clock clk ---"
report_clock_skew -setup -clock clk
puts "PASS: clock_skew named"

puts "--- report_clock_skew -digits 6 ---"
report_clock_skew -setup -digits 6
puts "PASS: clock_skew digits"

puts "--- report_clock_skew -include_internal_latency setup ---"
report_clock_skew -setup -include_internal_latency
puts "PASS: clock_skew internal_latency setup"

puts "--- report_clock_skew -include_internal_latency hold ---"
report_clock_skew -hold -include_internal_latency
puts "PASS: clock_skew internal_latency hold"

puts "--- report_clock_skew -digits 6 -include_internal_latency ---"
report_clock_skew -setup -digits 6 -include_internal_latency
puts "PASS: clock_skew digits + internal"

############################################################
# Clock latency and uncertainty affect skew
############################################################
puts "--- clock_latency + uncertainty ---"
set_clock_latency -source 0.3 [get_clocks clk]
set_clock_uncertainty -setup 0.2 [get_clocks clk]
set_clock_uncertainty -hold 0.1 [get_clocks clk]

report_clock_skew -setup
report_clock_skew -hold
report_clock_skew -setup -digits 4
puts "PASS: latency + uncertainty skew"

############################################################
# report_clock_latency
############################################################
puts "--- report_clock_latency ---"
report_clock_latency
puts "PASS: clock_latency"

puts "--- report_clock_latency -include_internal_latency ---"
report_clock_latency -include_internal_latency
puts "PASS: clock_latency internal"

puts "--- report_clock_latency -clock clk ---"
report_clock_latency -clock clk
puts "PASS: clock_latency named"

puts "--- report_clock_latency -digits 6 ---"
report_clock_latency -digits 6
puts "PASS: clock_latency digits"

############################################################
# report_clock_min_period
############################################################
puts "--- report_clock_min_period ---"
report_clock_min_period
puts "PASS: clock_min_period"

puts "--- report_clock_min_period -clocks clk ---"
report_clock_min_period -clocks clk
puts "PASS: clock_min_period named"

puts "--- report_clock_min_period -include_port_paths ---"
report_clock_min_period -include_port_paths
puts "PASS: clock_min_period port_paths"

############################################################
# find_clk_min_period
############################################################
puts "--- find_clk_min_period ---"
set mp1 [sta::find_clk_min_period [get_clocks clk] 0]
puts "clk min_period: $mp1"
set mp2 [sta::find_clk_min_period [get_clocks clk] 1]
puts "clk min_period (port): $mp2"
puts "PASS: find_clk_min_period"

############################################################
# Add multicycle
############################################################
puts "--- add multicycle ---"
set_multicycle_path -setup 2 -from [get_clocks clk] -to [get_clocks clk]
set_multicycle_path -hold 1 -from [get_clocks clk] -to [get_clocks clk]
report_checks -path_delay max
report_checks -path_delay min
puts "PASS: multicycle"

puts "--- skew after multicycle ---"
report_clock_skew -setup
report_clock_skew -hold
puts "PASS: skew after multicycle"

############################################################
# Clock transition
############################################################
puts "--- set_clock_transition ---"
set_clock_transition 0.1 [get_clocks clk]
report_checks -path_delay max -format full_clock_expanded
puts "PASS: clock transitions"

############################################################
# report_checks with full_clock and full_clock_expanded
############################################################
puts "--- report_checks -format full_clock ---"
report_checks -path_delay max -format full_clock
puts "PASS: full_clock"

puts "--- report_checks -format full_clock_expanded ---"
report_checks -path_delay min -format full_clock_expanded
puts "PASS: full_clock_expanded min"

############################################################
# Inter-clock uncertainty (same clock)
############################################################
puts "--- inter-clock uncertainty ---"
set_clock_uncertainty -from [get_clocks clk] -to [get_clocks clk] -setup 0.15
report_checks -path_delay max -format full_clock_expanded
puts "PASS: inter_clock_uncertainty"

############################################################
# Pulse width checks
############################################################
puts "--- report_pulse_width_checks ---"
report_pulse_width_checks
puts "PASS: pulse_width_checks"

puts "--- report_pulse_width_checks -verbose ---"
report_pulse_width_checks -verbose
puts "PASS: pulse_width_checks verbose"

############################################################
# Clock min period report
############################################################
puts "--- report_clock_min_period after multicycle ---"
report_clock_min_period
puts "PASS: clock_min_period after multicycle"

puts "ALL PASSED"
