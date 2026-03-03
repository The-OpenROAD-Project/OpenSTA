# Test ReportPath with PVT info, capacitance fields, and various
# PathEnd type reporting (latch, output delay, data check, unconstrained).
# Targets: ReportPath.cc reportPathPvt (corner PVT display),
#          reportPathCapacitance, reportFull for PathEndLatchCheck,
#          PathEndOutputDelay, PathEndUnconstrained, PathEndPathDelay,
#          reportSummaryLine, reportSlackOnly, reportEndpointHeader,
#          report_path_cmd with different formats,
#          PathEnd.cc various PathEnd type methods,
#          Sta.cc findPathEnds with complex filters,
#          endpointViolationCount, endpointPins, startpointPins
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_latch.v
link_design search_latch

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]

# Force timing
report_checks -path_delay max > /dev/null

############################################################
# Latch path timing (PathEndLatchCheck)
############################################################
puts "--- Latch timing: full format ---"
report_checks -path_delay max -format full

puts "--- Latch timing: full_clock format ---"
report_checks -path_delay max -format full_clock

puts "--- Latch timing: full_clock_expanded format ---"
report_checks -path_delay max -format full_clock_expanded

puts "--- Latch timing: short format ---"
report_checks -path_delay max -format short

puts "--- Latch timing: end format ---"
report_checks -path_delay max -format end

puts "--- Latch timing: summary format ---"
report_checks -path_delay max -format summary

puts "--- Latch timing: slack_only format ---"
report_checks -path_delay max -format slack_only

puts "--- Latch timing: json format ---"
report_checks -path_delay max -format json

puts "--- Latch timing min: all formats ---"
report_checks -path_delay min -format full
report_checks -path_delay min -format full_clock
report_checks -path_delay min -format full_clock_expanded
report_checks -path_delay min -format short
report_checks -path_delay min -format end
report_checks -path_delay min -format summary
report_checks -path_delay min -format slack_only
report_checks -path_delay min -format json

############################################################
# Capacitance field in report
############################################################
puts "--- report with capacitance field ---"
report_checks -path_delay max -fields {capacitance}

puts "--- report with all fields ---"
report_checks -path_delay max -fields {capacitance slew fanout input_pin net src_attr}

puts "--- report with capacitance + slew ---"
report_checks -path_delay max -fields {capacitance slew}

puts "--- report full_clock with capacitance ---"
report_checks -path_delay max -format full_clock -fields {capacitance}

puts "--- report full_clock_expanded with capacitance ---"
report_checks -path_delay max -format full_clock_expanded -fields {capacitance}

############################################################
# With loads set to exercise capacitance reporting
############################################################
puts "--- set_load and report with cap ---"
set_load 0.05 [get_ports out1]
set_load 0.03 [get_ports out2]
report_checks -path_delay max -fields {capacitance slew fanout}
report_checks -path_delay min -fields {capacitance slew fanout}

############################################################
# Output delay PathEnd (PathEndOutputDelay)
############################################################
puts "--- Output delay paths ---"
report_checks -to [get_ports out1] -path_delay max -format full
report_checks -to [get_ports out1] -path_delay min -format full
report_checks -to [get_ports out2] -path_delay max -format full
report_checks -to [get_ports out2] -path_delay min -format full

############################################################
# Unconstrained paths
############################################################
puts "--- unconstrained paths ---"
report_checks -unconstrained -path_delay max
report_checks -unconstrained -path_delay min
report_checks -unconstrained -path_delay max -format json

############################################################
# Max/min delay paths (PathEndPathDelay)
############################################################
puts "--- max_delay constraint ---"
set_max_delay -from [get_ports in1] -to [get_ports out1] 8.0
report_checks -path_delay max -format full
report_checks -path_delay max -format full_clock
report_checks -path_delay max -format json

puts "--- min_delay constraint ---"
set_min_delay -from [get_ports in1] -to [get_ports out1] 1.0
report_checks -path_delay min -format full
report_checks -path_delay min -format full_clock
report_checks -path_delay min -format json

############################################################
# find_timing_paths with various filters
############################################################
puts "--- find_timing_paths max ---"
set paths_max [find_timing_paths -path_delay max -endpoint_count 5 -group_path_count 10]
puts "Found [llength $paths_max] max paths"
foreach pe $paths_max {
  puts "  pin=[get_full_name [$pe pin]] slack=[$pe slack] is_check=[$pe is_check]"
}

puts "--- find_timing_paths min ---"
set paths_min [find_timing_paths -path_delay min -endpoint_count 5 -group_path_count 10]
puts "Found [llength $paths_min] min paths"
foreach pe $paths_min {
  puts "  pin=[get_full_name [$pe pin]] slack=[$pe slack] is_check=[$pe is_check]"
}

puts "--- find_timing_paths min_max ---"
set paths_mm [find_timing_paths -path_delay min_max -endpoint_count 3]
puts "Found [llength $paths_mm] min_max paths"
foreach pe $paths_mm {
  puts "  min_max=[$pe min_max] slack=[$pe slack]"
}

############################################################
# Endpoint and startpoint pins
############################################################
puts "--- endpoint/startpoint pins ---"
set ep [sta::endpoints]
puts "Endpoints: [llength $ep]"
# TODO: sta::startpoints removed from SWIG interface (startpointPins not defined)
# set sp [sta::startpoints]
# puts "Startpoints: [llength $sp]"
puts "Startpoints: skipped (API removed)"
set ep_count [sta::endpoint_count]
puts "Endpoint count: $ep_count"

############################################################
# Endpoint violation count
############################################################
puts "--- endpoint_violation_count ---"
set viol_max [sta::endpoint_violation_count "max"]
puts "Max violations: $viol_max"
set viol_min [sta::endpoint_violation_count "min"]
puts "Min violations: $viol_min"

############################################################
# report_path with digits
############################################################
puts "--- report with high digits ---"
report_checks -path_delay max -digits 8

puts "--- report with low digits ---"
report_checks -path_delay max -digits 2

############################################################
# TotalNegativeSlack and WorstSlack
############################################################
puts "--- TotalNegativeSlack ---"
report_tns
report_wns
report_worst_slack -max
report_worst_slack -min
