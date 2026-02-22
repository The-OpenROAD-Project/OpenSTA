# Test data check constraints, gated clock paths, recovery/removal,
# PathEndDataCheck, PathEndGatedClock reporting.
# Targets: ReportPath.cc reportShort/reportFull/reportEndpoint for
#          PathEndDataCheck, PathEndGatedClock, PathEnd.cc PathEndDataCheck,
#          PathEndGatedClock, Genclks.cc, GatedClk.cc, ClkSkew.cc
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

puts "--- Basic timing ---"
report_checks -path_delay max
report_checks -path_delay min

puts "--- Enable gated clock checks ---"
sta::set_gated_clk_checks_enabled 1
report_checks -path_delay max -format full_clock_expanded
report_checks -path_delay min -format full_clock_expanded

puts "--- find_timing_paths with gated clk ---"
set paths [find_timing_paths -path_delay max -endpoint_path_count 10 -group_path_count 20]
puts "Found [llength $paths] max paths"
foreach pe $paths {
  puts "  is_gated: [$pe is_gated_clock] is_check: [$pe is_check] pin=[get_full_name [$pe pin]] role=[$pe check_role] slack=[$pe slack]"
}

puts "--- report_checks in various formats with gated clk ---"
report_checks -path_delay max -format full
report_checks -path_delay max -format full_clock
report_checks -path_delay max -format short
report_checks -path_delay max -format end
report_checks -path_delay max -format summary
report_checks -path_delay max -format slack_only
report_checks -path_delay max -format json

puts "--- propagate_gated_clock_enable ---"
sta::set_propagate_gated_clock_enable 1
report_checks -path_delay max
report_checks -path_delay min
sta::set_propagate_gated_clock_enable 0

sta::set_gated_clk_checks_enabled 0

puts "--- Enable recovery/removal checks ---"
sta::set_recovery_removal_checks_enabled 1
report_checks -path_delay max -format full_clock_expanded
report_checks -path_delay min -format full_clock_expanded

puts "--- find_timing_paths with recovery/removal ---"
set paths_rr [find_timing_paths -path_delay max -endpoint_path_count 15 -group_path_count 30]
puts "Found [llength $paths_rr] paths with recovery/removal"
foreach pe $paths_rr {
  set role [$pe check_role]
  puts "  role=$role is_check=[$pe is_check] pin=[get_full_name [$pe pin]] slack=[$pe slack]"
}

puts "--- report recovery/removal in formats ---"
report_checks -path_delay max -format full
report_checks -path_delay max -format json
report_checks -path_delay min -format full_clock

sta::set_recovery_removal_checks_enabled 0

puts "--- Data check constraints ---"
set_data_check -from [get_pins reg1/CK] -to [get_pins reg2/D] -setup 0.2
set_data_check -from [get_pins reg1/CK] -to [get_pins reg2/D] -hold 0.1
report_checks -path_delay max -format full_clock_expanded
report_checks -path_delay min -format full_clock_expanded
puts "--- find_timing_paths with data check ---"
set paths_dc [find_timing_paths -path_delay max -endpoint_path_count 10]
puts "Found [llength $paths_dc] paths with data check"
foreach pe $paths_dc {
  puts "  is_data_check: [$pe is_data_check] pin=[get_full_name [$pe pin]] role=[$pe check_role]"
}

puts "--- clock skew analysis ---"
report_clock_skew -setup
report_clock_skew -hold
report_clock_skew -setup -include_internal_latency
report_clock_skew -hold -include_internal_latency
report_clock_skew -setup -digits 6
report_clock_skew -hold -digits 6

puts "--- clock latency ---"
report_clock_latency
report_clock_latency -include_internal_latency
report_clock_latency -digits 6

puts "--- worst_clk_skew_cmd ---"
set ws_setup [sta::worst_clk_skew_cmd setup 0]
puts "Worst skew setup: $ws_setup"
set ws_hold [sta::worst_clk_skew_cmd hold 0]
puts "Worst skew hold: $ws_hold"
set ws_setup_int [sta::worst_clk_skew_cmd setup 1]
puts "Worst skew setup (int): $ws_setup_int"
set ws_hold_int [sta::worst_clk_skew_cmd hold 1]
puts "Worst skew hold (int): $ws_hold_int"

puts "--- report_check_types with everything ---"
report_check_types -verbose
report_check_types -violators

puts "--- check_setup ---"
check_setup -verbose
check_setup -verbose -no_input_delay
check_setup -verbose -no_output_delay
check_setup -verbose -unconstrained_endpoints
check_setup -verbose -no_clock
check_setup -verbose -loops
check_setup -verbose -generated_clocks

puts "--- clock properties ---"
report_clock_properties
report_clock_properties clk

puts "--- find_timing_paths with -through ---"
set paths_thru [find_timing_paths -through [get_pins clk_gate/ZN] -path_delay max]
puts "Paths through clk_gate/ZN: [llength $paths_thru]"

puts "--- report_checks -through ---"
report_checks -through [get_pins buf1/Z] -path_delay max
report_checks -through [get_pins inv1/ZN] -path_delay max

puts "--- pulse width checks ---"
report_pulse_width_checks
report_pulse_width_checks -verbose

puts "--- min period ---"
report_clock_min_period
report_clock_min_period -include_port_paths
