# Test Levelize.cc: levelization with disabled arcs, loop detection,
# re-levelization after constraint changes, and level query operations.
# Uses search_check_timing.v (has multiple clocks).
# Targets: Levelize.cc levelize, ensureLevelized,
#   levelizeFrom, isDisabledLoop, checkLoops, reportLoops,
#   relevelize, clearLogicValues, graphChangedAfterLevelize,
#   Search.cc levelize, levelizeGraph, reportLoops
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_check_timing.v
link_design search_check_timing

create_clock -name clk -period 10 [get_ports clk]
create_clock -name clk2 -period 15 [get_ports clk2]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_input_delay -clock clk 1.0 [get_ports in3]
set_input_delay -clock clk 0.5 [get_ports in_unconst]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]

# Run initial timing (triggers levelize)
puts "--- initial timing ---"
report_checks -path_delay max
report_checks -path_delay min
puts "PASS: initial timing"

############################################################
# Disable timing arcs and re-run (triggers re-levelize)
############################################################
puts "--- disable_timing ---"
set_disable_timing [get_cells buf1] -from A -to Z
report_checks -path_delay max
puts "PASS: disable_timing"

puts "--- enable_timing ---"
unset_disable_timing [get_cells buf1] -from A -to Z
report_checks -path_delay max
puts "PASS: enable_timing"

############################################################
# set_disable_timing on lib cell arcs
############################################################
puts "--- disable lib cell arcs ---"
set_disable_timing [get_lib_cells Nangate45/AND2_X1] -from A1 -to ZN
report_checks -path_delay max
puts "PASS: disable lib cell arcs"

puts "--- re-enable lib cell arcs ---"
unset_disable_timing [get_lib_cells Nangate45/AND2_X1] -from A1 -to ZN
report_checks -path_delay max
puts "PASS: re-enable lib cell arcs"

############################################################
# set_case_analysis (can cause logic constant propagation
# which triggers levelize through sim values)
############################################################
puts "--- set_case_analysis ---"
set_case_analysis 1 [get_ports in2]
report_checks -path_delay max
puts "PASS: case_analysis"

puts "--- remove case_analysis ---"
unset_case_analysis [get_ports in2]
report_checks -path_delay max
puts "PASS: remove case_analysis"

############################################################
# check_setup to report unconstrained endpoints
############################################################
puts "--- check_setup ---"
check_setup -verbose
puts "PASS: check_setup verbose"

############################################################
# report_disabled_edges
############################################################
puts "--- report_disabled_edges ---"
report_disabled_edges
puts "PASS: report_disabled_edges"

############################################################
# Timing with different clock uncertainties (triggers
# re-search which exercises levelize + tag operations)
############################################################
puts "--- inter-clock uncertainty ---"
set_clock_uncertainty -setup 0.5 -from [get_clocks clk] -to [get_clocks clk2]
set_clock_uncertainty -hold 0.2 -from [get_clocks clk] -to [get_clocks clk2]
report_checks -path_delay max
report_checks -path_delay min
puts "PASS: inter-clock uncertainty"

############################################################
# Disable whole instance (all arcs)
############################################################
puts "--- disable_timing whole instance ---"
set_disable_timing [get_cells and1]
report_checks -path_delay max
puts "PASS: disable whole instance"

puts "--- enable_timing whole instance ---"
unset_disable_timing [get_cells and1]
report_checks -path_delay max
puts "PASS: enable whole instance"

############################################################
# check_setup with loop detection
############################################################
puts "--- check_setup -loops ---"
check_setup -verbose -loops
puts "PASS: loop check"

############################################################
# Multiple timing operations that force re-levelize
############################################################
puts "--- multiple re-levelize triggers ---"
set_false_path -from [get_ports in1] -to [get_pins reg1/D]
report_checks -path_delay max
unset_path_exceptions -from [get_ports in1] -to [get_pins reg1/D]
report_checks -path_delay max
set_multicycle_path 2 -setup -from [get_clocks clk] -to [get_clocks clk2]
report_checks -path_delay max
puts "PASS: multiple re-levelize"

############################################################
# Disable timing on ports
############################################################
puts "--- disable timing on port ---"
set_disable_timing [get_ports in1]
report_checks -path_delay max
puts "PASS: disable port"

puts "--- enable timing on port ---"
unset_disable_timing [get_ports in1]
report_checks -path_delay max
puts "PASS: enable port"

############################################################
# set_case_analysis 0 (different constant)
############################################################
puts "--- set_case_analysis 0 ---"
set_case_analysis 0 [get_ports in1]
report_checks -path_delay max
puts "PASS: case_analysis 0"

puts "--- remove case_analysis 0 ---"
unset_case_analysis [get_ports in1]
report_checks -path_delay max
puts "PASS: remove case_analysis 0"

############################################################
# Multiple clock domains with different constraints
############################################################
puts "--- add constraints and re-analyze ---"
set_clock_latency -source 0.3 [get_clocks clk]
set_clock_latency -source 0.5 [get_clocks clk2]
set_clock_transition 0.1 [get_clocks clk]
report_checks -path_delay max -format full_clock_expanded
report_checks -path_delay min -format full_clock_expanded
puts "PASS: multi-constraint re-analysis"

puts "ALL PASSED"
