# Test ReportPath.cc: Gated clock and data check path reporting,
# reportFull/reportShort/reportEndpoint for PathEndGatedClock
# and PathEndDataCheck, reportEndLine/reportSummaryLine for these types,
# report_path_end chaining with mixed PathEnd types,
# report_path_ends with gated clock paths.
# Also exercises reportEndpointOutputDelay, reportPathDelay,
# PathEnd type predicates (is_gated_clock, is_data_check, is_output_delay).
# Targets: ReportPath.cc reportFull(PathEndGatedClock),
#          reportShort(PathEndGatedClock), reportEndpoint(PathEndGatedClock),
#          reportFull(PathEndDataCheck), reportShort(PathEndDataCheck),
#          reportEndpoint(PathEndDataCheck), reportEndLine, reportSummaryLine,
#          checkRoleString, checkRoleReason
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

############################################################
# Enable gated clock checks
############################################################
puts "--- Gated clock full_clock_expanded with fields ---"
sta::set_gated_clk_checks_enabled 1
report_checks -path_delay max -format full_clock_expanded -fields {capacitance slew fanout input_pin net src_attr}
report_checks -path_delay min -format full_clock_expanded -fields {capacitance slew fanout input_pin net src_attr}

############################################################
# Gated clock path iteration with detailed properties
############################################################
puts "--- Gated clock path detail ---"
set gated_paths [find_timing_paths -path_delay max -endpoint_path_count 15 -group_path_count 30]
puts "Total max paths: [llength $gated_paths]"
foreach pe $gated_paths {
  puts "  type: is_gated=[$pe is_gated_clock] is_check=[$pe is_check] is_output=[$pe is_output_delay] is_latch=[$pe is_latch_check] is_data=[$pe is_data_check] is_path_delay=[$pe is_path_delay] is_uncon=[$pe is_unconstrained]"
  puts "    pin=[get_full_name [$pe pin]] role=[$pe check_role] slack=[$pe slack]"
  puts "    margin=[$pe margin] data_arr=[$pe data_arrival_time] data_req=[$pe data_required_time]"
  puts "    target_clk: [get_name [$pe target_clk]]"
  puts "    target_clk_time: [$pe target_clk_time]"
  puts "    end_transition: [$pe end_transition]"
  puts "    min_max: [$pe min_max]"
}

############################################################
# Gated clock in all report formats
############################################################
puts "--- Gated clock all formats ---"
report_checks -path_delay max -format full
report_checks -path_delay max -format full_clock
report_checks -path_delay max -format short
report_checks -path_delay max -format end
report_checks -path_delay max -format summary
report_checks -path_delay max -format slack_only
report_checks -path_delay max -format json

puts "--- Gated clock min all formats ---"
report_checks -path_delay min -format full
report_checks -path_delay min -format full_clock
report_checks -path_delay min -format short
report_checks -path_delay min -format end
report_checks -path_delay min -format summary
report_checks -path_delay min -format slack_only

sta::set_gated_clk_checks_enabled 0

############################################################
# Enable recovery/removal checks
############################################################
puts "--- Recovery/removal full_clock_expanded with fields ---"
sta::set_recovery_removal_checks_enabled 1
report_checks -path_delay max -format full_clock_expanded -fields {capacitance slew fanout input_pin net}
report_checks -path_delay min -format full_clock_expanded -fields {capacitance slew fanout input_pin net}

puts "--- Recovery/removal formats ---"
report_checks -path_delay max -format full
report_checks -path_delay max -format full_clock
report_checks -path_delay max -format short
report_checks -path_delay max -format end
report_checks -path_delay max -format summary
report_checks -path_delay max -format json

puts "--- Recovery path iteration ---"
set recov_paths [find_timing_paths -path_delay max -endpoint_path_count 15 -group_path_count 30]
puts "Recovery max paths: [llength $recov_paths]"
foreach pe $recov_paths {
  set role [$pe check_role]
  puts "  role=$role is_check=[$pe is_check] pin=[get_full_name [$pe pin]] slack=[$pe slack]"
}

sta::set_recovery_removal_checks_enabled 0

############################################################
# Data check constraints with reporting
############################################################
puts "--- Data check with full_clock_expanded ---"
set_data_check -from [get_pins reg1/CK] -to [get_pins reg2/D] -setup 0.3
set_data_check -from [get_pins reg1/CK] -to [get_pins reg2/D] -hold 0.15
report_checks -path_delay max -format full_clock_expanded -fields {capacitance slew fanout}
report_checks -path_delay min -format full_clock_expanded -fields {capacitance slew fanout}
puts "data check full_clock_expanded done"

puts "--- Data check all formats ---"
report_checks -path_delay max -format full
report_checks -path_delay max -format full_clock
report_checks -path_delay max -format short
report_checks -path_delay max -format end
report_checks -path_delay max -format summary
report_checks -path_delay max -format slack_only
report_checks -path_delay max -format json
puts "data check all formats done"

puts "--- Data check path iteration ---"
set dc_paths [find_timing_paths -path_delay max -endpoint_path_count 10]
puts "Data check max paths: [llength $dc_paths]"
foreach pe $dc_paths {
  puts "  is_data_check: [$pe is_data_check] role=[$pe check_role] pin=[get_full_name [$pe pin]]"
}

############################################################
# Propagated gated clock enable
############################################################
puts "--- Propagate gated clock enable ---"
sta::set_gated_clk_checks_enabled 1
sta::set_propagate_gated_clock_enable 1
report_checks -path_delay max -format full_clock_expanded
report_checks -path_delay min -format full_clock_expanded
sta::set_propagate_gated_clock_enable 0
sta::set_gated_clk_checks_enabled 0

############################################################
# Report with -digits and -no_line_splits
############################################################
puts "--- Digits and no_line_splits ---"
report_checks -path_delay max -digits 6 -format full_clock_expanded
report_checks -path_delay max -no_line_splits -format full_clock_expanded

############################################################
# report_checks -unconstrained
############################################################
puts "--- unconstrained ---"
report_checks -path_delay max -unconstrained

############################################################
# endpoint_violation_count
############################################################
puts "--- endpoint_violation_count ---"
puts "max violations: [sta::endpoint_violation_count max]"
puts "min violations: [sta::endpoint_violation_count min]"

############################################################
# Startpoints / endpoints
############################################################
puts "--- startpoints / endpoints ---"
set starts [sta::startpoints]
puts "startpoints: [llength $starts]"
set ends [sta::endpoints]
puts "endpoints: [llength $ends]"
