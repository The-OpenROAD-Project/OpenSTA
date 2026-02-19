# Test CheckTiming.cc, GatedClk.cc, and various check commands
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_check_timing.v
link_design search_check_timing

create_clock -name clk -period 10 [get_ports clk]
create_clock -name clk2 -period 15 [get_ports clk2]

# Intentionally leave some ports without delays for check_setup testing
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
# in3 and in_unconst deliberately have no input delay
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk2 2.0 [get_ports out2]
# out_unconst deliberately has no output delay

puts "--- check_setup all flags ---"
check_setup -verbose

puts "--- check_setup -no_input_delay ---"
check_setup -verbose -no_input_delay

puts "--- check_setup -no_output_delay ---"
check_setup -verbose -no_output_delay

puts "--- check_setup -no_clock ---"
check_setup -verbose -no_clock

puts "--- check_setup -unconstrained_endpoints ---"
check_setup -verbose -unconstrained_endpoints

puts "--- check_setup -loops ---"
check_setup -verbose -loops

puts "--- check_setup -generated_clocks ---"
check_setup -verbose -generated_clocks

puts "--- check_setup multiple flags combined ---"
check_setup -verbose -no_input_delay -no_output_delay -unconstrained_endpoints -loops -generated_clocks

puts "--- report_check_types all ---"
report_check_types -verbose

puts "--- report_check_types individual ---"
report_check_types -max_delay
report_check_types -min_delay
report_check_types -max_slew
report_check_types -max_fanout
report_check_types -max_capacitance
report_check_types -min_pulse_width
report_check_types -min_period
report_check_types -max_skew
report_check_types -violators

puts "--- set_max_transition and check ---"
set_max_transition 0.5 [current_design]
report_check_types -max_slew -verbose

puts "--- set_max_capacitance and check ---"
set_max_capacitance 0.05 [current_design]
report_check_types -max_capacitance -verbose

puts "--- set_max_fanout and check ---"
set_max_fanout 10 [current_design]
report_check_types -max_fanout -verbose

puts "--- report_checks with unconstrained ---"
report_checks -unconstrained -path_delay max

puts "--- set_clock_groups and check ---"
set_clock_groups -name cg1 -asynchronous -group {clk} -group {clk2}
report_checks -path_delay max
check_setup -verbose
unset_clock_groups -asynchronous -name cg1

puts "--- report_clock_min_period ---"
report_clock_min_period
report_clock_min_period -include_port_paths
report_clock_min_period -clocks clk
report_clock_min_period -clocks clk2

puts "--- Gated clock enable settings ---"
sta::set_propagate_gated_clock_enable 1
puts "gated_clk_enable: [sta::propagate_gated_clock_enable]"
report_checks -path_delay max
sta::set_propagate_gated_clock_enable 0

puts "--- Gated clock check settings ---"
sta::set_gated_clk_checks_enabled 1
puts "gated_clk_checks: [sta::gated_clk_checks_enabled]"
report_checks -path_delay max
sta::set_gated_clk_checks_enabled 0

puts "--- Recovery/removal checks ---"
sta::set_recovery_removal_checks_enabled 1
puts "recovery_removal: [sta::recovery_removal_checks_enabled]"
report_checks -path_delay max
sta::set_recovery_removal_checks_enabled 0

puts "--- Various STA variable settings ---"
sta::set_preset_clr_arcs_enabled 1
puts "preset_clr: [sta::preset_clr_arcs_enabled]"
sta::set_preset_clr_arcs_enabled 0

sta::set_cond_default_arcs_enabled 1
puts "cond_default: [sta::cond_default_arcs_enabled]"
sta::set_cond_default_arcs_enabled 0

sta::set_bidirect_inst_paths_enabled 1
puts "bidirect_inst: [sta::bidirect_inst_paths_enabled]"
sta::set_bidirect_inst_paths_enabled 0

sta::set_bidirect_net_paths_enabled 1
puts "bidirect_net: [sta::bidirect_net_paths_enabled]"
sta::set_bidirect_net_paths_enabled 0

sta::set_dynamic_loop_breaking 1
puts "dynamic_loop: [sta::dynamic_loop_breaking]"
sta::set_dynamic_loop_breaking 0

sta::set_use_default_arrival_clock 1
puts "default_arrival_clk: [sta::use_default_arrival_clock]"
sta::set_use_default_arrival_clock 0
