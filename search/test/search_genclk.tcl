# Test generated clocks
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_genclk.v
link_design search_genclk

# Create base clock
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

puts "--- create_generated_clock -divide_by 2 ---"
create_generated_clock -name div_clk -source [get_pins clkbuf/Z] -divide_by 2 [get_pins div_reg/Q]
set_output_delay -clock div_clk 1.0 [get_ports out2]

puts "--- report_clock_properties ---"
report_clock_properties

puts "--- report_clock_properties div_clk ---"
report_clock_properties div_clk

puts "--- report_checks max ---"
report_checks -path_delay max

puts "--- report_checks min ---"
report_checks -path_delay min

puts "--- report_checks through generated clock domain ---"
report_checks -to [get_ports out2] -path_delay max

puts "--- report_checks -format full_clock for genclk path ---"
report_checks -to [get_ports out2] -format full_clock

puts "--- report_checks -format full_clock_expanded for genclk path ---"
report_checks -to [get_ports out2] -format full_clock_expanded

puts "--- report_clock_skew setup ---"
report_clock_skew -setup

puts "--- report_clock_skew hold ---"
report_clock_skew -hold

puts "--- report_clock_latency ---"
report_clock_latency

puts "--- report_clock_min_period ---"
report_clock_min_period

puts "--- report_clock_min_period -clocks ---"
report_clock_min_period -clocks clk

puts "--- report_clock_min_period div_clk ---"
report_clock_min_period -clocks div_clk

puts "--- check_setup for generated clocks ---"
check_setup -verbose

puts "--- check_setup -generated_clocks ---"
check_setup -verbose -generated_clocks

puts "--- report_check_types verbose ---"
report_check_types -verbose

puts "--- report_tns ---"
report_tns -max

puts "--- report_wns ---"
report_wns -max

puts "--- report_worst_slack ---"
report_worst_slack -max

puts "--- find_timing_paths through div_clk domain ---"
set paths [find_timing_paths -to [get_ports out2] -path_delay max]
puts "Found [llength $paths] paths to out2"

puts "--- set_clock_groups -logically_exclusive ---"
set_clock_groups -name clk_le -logically_exclusive -group {clk} -group {div_clk}
report_checks -path_delay max

puts "--- unset_clock_groups ---"
unset_clock_groups -logically_exclusive -name clk_le

puts "--- set_clock_groups -asynchronous ---"
set_clock_groups -name clk_async -asynchronous -group {clk} -group {div_clk}
report_checks -path_delay max

puts "--- unset_clock_groups -asynchronous ---"
unset_clock_groups -asynchronous -name clk_async

puts "--- delete generated clock and create multiply_by ---"
delete_generated_clock div_clk
create_generated_clock -name fast_clk -source [get_pins clkbuf/Z] -multiply_by 2 [get_pins div_reg/Q]
set_output_delay -clock fast_clk 0.5 [get_ports out2]

puts "--- report_clock_properties after multiply_by ---"
report_clock_properties

puts "--- report_checks with multiply_by clock ---"
report_checks -path_delay max

puts "--- report_checks to out2 with fast_clk ---"
report_checks -to [get_ports out2] -path_delay max

puts "--- report_clock_min_period for fast_clk ---"
report_clock_min_period -clocks fast_clk

puts "--- set_clock_uncertainty on generated clock ---"
set_clock_uncertainty 0.1 [get_clocks fast_clk]
report_checks -path_delay max -to [get_ports out2]

puts "--- set_clock_latency -source on generated clock ---"
set_clock_latency -source 0.15 [get_clocks fast_clk]
report_checks -path_delay max -to [get_ports out2]

puts "--- report_pulse_width_checks ---"
report_pulse_width_checks

puts "--- report_pulse_width_checks verbose ---"
report_pulse_width_checks -verbose
