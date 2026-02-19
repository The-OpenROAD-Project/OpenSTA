# Test ClkSkew.cc with multiple clocks of different periods:
# inter-clock skew reporting, findClkSkew with different clock domains,
# reportClkSkew with multi-clock iteration, clock pair iteration.
# Uses sdc_test2.v which has two clock domains (clk1, clk2).
# Targets: ClkSkew.cc ClkSkew constructor with different clocks,
#   srcLatency, tgtLatency for different domain skews,
#   findClkSkew multi-clock, reportClkSkew with -clock filtering,
#   Sta.cc reportClockSkew multi-clock, reportClockLatency multi-clock,
#   reportClockMinPeriod multi-clock
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog ../../sdc/test/sdc_test2.v
link_design sdc_test2

############################################################
# Setup two clocks with different periods
############################################################
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 15 [get_ports clk2]

set_input_delay -clock clk1 1.0 [get_ports in1]
set_input_delay -clock clk1 1.0 [get_ports in2]
set_input_delay -clock clk2 2.0 [get_ports in3]
set_output_delay -clock clk1 2.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]
set_input_transition 0.1 [all_inputs]

report_checks > /dev/null

############################################################
# report_clock_skew with multiple clocks
############################################################
puts "--- report_clock_skew -setup (all clocks) ---"
report_clock_skew -setup

puts "--- report_clock_skew -hold (all clocks) ---"
report_clock_skew -hold

puts "--- report_clock_skew -clock clk1 -setup ---"
report_clock_skew -setup -clock clk1

puts "--- report_clock_skew -clock clk2 -setup ---"
report_clock_skew -setup -clock clk2

puts "--- report_clock_skew -clock clk1 -hold ---"
report_clock_skew -hold -clock clk1

puts "--- report_clock_skew -clock clk2 -hold ---"
report_clock_skew -hold -clock clk2

puts "--- report_clock_skew -digits 6 ---"
report_clock_skew -setup -digits 6

puts "--- report_clock_skew -include_internal_latency ---"
report_clock_skew -setup -include_internal_latency

puts "--- report_clock_skew -hold -include_internal_latency ---"
report_clock_skew -hold -include_internal_latency

############################################################
# report_clock_latency for multiple clocks
############################################################
puts "--- report_clock_latency (all clocks) ---"
report_clock_latency

puts "--- report_clock_latency -clock clk1 ---"
report_clock_latency -clock clk1

puts "--- report_clock_latency -clock clk2 ---"
report_clock_latency -clock clk2

puts "--- report_clock_latency -include_internal_latency ---"
report_clock_latency -include_internal_latency

puts "--- report_clock_latency -digits 6 ---"
report_clock_latency -digits 6

############################################################
# report_clock_min_period for multiple clocks
############################################################
puts "--- report_clock_min_period (all clocks) ---"
report_clock_min_period

puts "--- report_clock_min_period -clocks clk1 ---"
report_clock_min_period -clocks clk1

puts "--- report_clock_min_period -clocks clk2 ---"
report_clock_min_period -clocks clk2

puts "--- report_clock_min_period -include_port_paths ---"
report_clock_min_period -include_port_paths

############################################################
# find_clk_min_period for each clock
############################################################
puts "--- find_clk_min_period clk1 ---"
set mp1 [sta::find_clk_min_period [get_clocks clk1] 0]
puts "clk1 min_period: $mp1"
set mp1p [sta::find_clk_min_period [get_clocks clk1] 1]
puts "clk1 min_period (port): $mp1p"

puts "--- find_clk_min_period clk2 ---"
set mp2 [sta::find_clk_min_period [get_clocks clk2] 0]
puts "clk2 min_period: $mp2"
set mp2p [sta::find_clk_min_period [get_clocks clk2] 1]
puts "clk2 min_period (port): $mp2p"

############################################################
# Add clock latency and uncertainty for inter-clock scenarios
############################################################
puts "--- clock latency + uncertainty ---"
set_clock_latency -source 0.3 [get_clocks clk1]
set_clock_latency -source 0.5 [get_clocks clk2]
set_clock_uncertainty -setup 0.2 -from [get_clocks clk1] -to [get_clocks clk2]
set_clock_uncertainty -hold 0.1 -from [get_clocks clk1] -to [get_clocks clk2]

report_checks -path_delay max
report_checks -path_delay min

puts "--- clock skew after latency+uncertainty ---"
report_clock_skew -setup
report_clock_skew -hold

############################################################
# Set clock groups and check skew
############################################################
puts "--- clock_groups ---"
set_clock_groups -asynchronous -name async_cg -group [get_clocks clk1] -group [get_clocks clk2]
report_checks -path_delay max
report_clock_skew -setup

puts "--- remove clock_groups ---"
unset_clock_groups -asynchronous -name async_cg
report_checks -path_delay max

############################################################
# Pulse width checks with multi clock
############################################################
puts "--- report_pulse_width_checks multi-clock ---"
report_pulse_width_checks

puts "--- report_pulse_width_checks -verbose multi-clock ---"
report_pulse_width_checks -verbose

############################################################
# report_clock_properties
############################################################
puts "--- report_clock_properties ---"
report_clock_properties
