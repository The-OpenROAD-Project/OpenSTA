# Test deeper Genclks.cc paths, latch timing (Latches.cc),
# unconstrained path reporting, and PathEnd subclass coverage.
# Targets: Genclks.cc srcFilter, updateGeneratedClks, srcPath,
#          GenclkInfo, GenclkSrcArrivalVisitor,
#          PathEnd.cc PathEndLatchCheck, PathEndGatedClock,
#          PathEndUnconstrained, PathEndOutputDelay,
#          ReportPath.cc reportShort/Full for latch, gated clock,
#          unconstrained, output delay path ends,
#          reportGenClkSrcAndPath, reportGenClkSrcPath1,
#          reportUnclockedEndpoint,
#          VisitPathEnds.cc clockGatingMargin,
#          Search.cc pathClkPathArrival
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_latch.v
link_design search_latch

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]

report_checks > /dev/null

############################################################
# Latch timing checks
############################################################
puts "--- report_checks max (latch paths) ---"
report_checks -path_delay max

puts "--- report_checks min (latch paths) ---"
report_checks -path_delay min

puts "--- report_checks -format full_clock ---"
report_checks -path_delay max -format full_clock

puts "--- report_checks -format full_clock_expanded ---"
report_checks -path_delay max -format full_clock_expanded

puts "--- report_checks -format short ---"
report_checks -path_delay max -format short

puts "--- report_checks -format end ---"
report_checks -path_delay max -format end

puts "--- report_checks -format summary ---"
report_checks -path_delay max -format summary

puts "--- report_checks -format slack_only ---"
report_checks -path_delay max -format slack_only

puts "--- report_checks -format json ---"
report_checks -path_delay max -format json

############################################################
# PathEnd type queries on latch paths
############################################################
puts "--- PathEnd queries on latch ---"
set paths_latch [find_timing_paths -path_delay max -endpoint_count 10 -group_path_count 20]
puts "Found [llength $paths_latch] paths"
foreach pe $paths_latch {
  set is_latch [$pe is_latch_check]
  set is_check [$pe is_check]
  set is_output [$pe is_output_delay]
  puts "  pin=[get_full_name [$pe pin]] latch=$is_latch check=$is_check output=$is_output slack=[$pe slack]"
}

############################################################
# report_checks with -fields for latch paths
############################################################
puts "--- report_checks with fields for latch ---"
report_checks -path_delay max -fields {capacitance slew fanout input_pin}

############################################################
# report_checks to specific output with latch
############################################################
puts "--- report_checks to out1 (through latch) ---"
report_checks -to [get_ports out1] -path_delay max -format full_clock

puts "--- report_checks to out2 (through reg) ---"
report_checks -to [get_ports out2] -path_delay max -format full_clock

############################################################
# Unconstrained paths
############################################################
puts "--- report_checks -unconstrained ---"
report_checks -path_delay max -unconstrained

puts "--- report_checks -unconstrained -format short ---"
report_checks -path_delay max -unconstrained -format short

puts "--- report_checks -unconstrained -format end ---"
report_checks -path_delay max -unconstrained -format end

puts "--- report_checks -unconstrained -format summary ---"
report_checks -path_delay max -unconstrained -format summary

############################################################
# Latch borrow limit
############################################################
puts "--- set_max_time_borrow ---"
set_max_time_borrow 3.0 [get_clocks clk]
report_checks -path_delay max -format full_clock

############################################################
# Now test with genclk design
############################################################
puts "--- Switch to genclk design ---"
read_verilog search_genclk.v
link_design search_genclk

create_clock -name clk -period 10 [get_ports clk]
create_generated_clock -name div_clk -source [get_pins clkbuf/Z] -divide_by 2 [get_pins div_reg/Q]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock div_clk 1.0 [get_ports out2]

############################################################
# Generated clock timing
############################################################
puts "--- genclk report_checks max ---"
report_checks -path_delay max

puts "--- genclk report_checks min ---"
report_checks -path_delay min

puts "--- genclk to div_clk domain ---"
report_checks -to [get_ports out2] -path_delay max

puts "--- genclk full_clock_expanded to div_clk domain ---"
report_checks -to [get_ports out2] -format full_clock_expanded

puts "--- genclk full_clock to div_clk domain ---"
report_checks -to [get_ports out2] -format full_clock

############################################################
# multiply_by generated clock
############################################################
puts "--- delete_generated_clock and create multiply_by ---"
delete_generated_clock div_clk
create_generated_clock -name fast_clk -source [get_pins clkbuf/Z] -multiply_by 2 [get_pins div_reg/Q]
set_output_delay -clock fast_clk 0.5 [get_ports out2]

puts "--- multiply_by clock reports ---"
report_checks -path_delay max
report_checks -to [get_ports out2] -format full_clock_expanded

############################################################
# Generated clock with edges specification
############################################################
puts "--- delete and create with -edges ---"
delete_generated_clock fast_clk
create_generated_clock -name edge_clk -source [get_pins clkbuf/Z] -edges {1 2 3} [get_pins div_reg/Q]
set_output_delay -clock edge_clk 0.5 [get_ports out2]

report_checks -path_delay max
report_checks -to [get_ports out2] -format full_clock_expanded
report_clock_properties

############################################################
# Clock groups with generated clocks
############################################################
puts "--- set_clock_groups ---"
set_clock_groups -name cg1 -logically_exclusive -group {clk} -group {edge_clk}
report_checks -path_delay max

unset_clock_groups -logically_exclusive -name cg1

puts "--- set_clock_groups -physically_exclusive ---"
set_clock_groups -name cg2 -physically_exclusive -group {clk} -group {edge_clk}
report_checks -path_delay max

unset_clock_groups -physically_exclusive -name cg2

puts "--- set_clock_groups -asynchronous ---"
set_clock_groups -name cg3 -asynchronous -group {clk} -group {edge_clk}
report_checks -path_delay max

unset_clock_groups -asynchronous -name cg3

############################################################
# Clock latency/uncertainty on generated clock
############################################################
puts "--- clock_latency on genclk ---"
set_clock_latency -source 0.15 [get_clocks edge_clk]
set_clock_uncertainty 0.1 [get_clocks edge_clk]
report_checks -path_delay max -to [get_ports out2]

############################################################
# Clock skew with genclk
############################################################
puts "--- clock_skew with genclk ---"
report_clock_skew -setup
report_clock_skew -hold

############################################################
# Clock min period with genclk
############################################################
puts "--- clock_min_period genclk ---"
report_clock_min_period
report_clock_min_period -clocks edge_clk

############################################################
# check_setup with generated clocks
############################################################
puts "--- check_setup genclk ---"
check_setup -verbose
check_setup -verbose -generated_clocks

############################################################
# report_check_types on genclk design
############################################################
puts "--- report_check_types on genclk ---"
report_check_types -verbose

############################################################
# find_timing_paths with various options
############################################################
puts "--- find_timing_paths -unique_edges_to_endpoint ---"
set ue_paths [find_timing_paths -unique_edges_to_endpoint -path_delay max -group_path_count 10 -endpoint_count 5]
puts "unique edge paths: [llength $ue_paths]"

puts "--- find_timing_paths min_max ---"
set mm_paths [find_timing_paths -path_delay min_max -group_path_count 5 -endpoint_count 3]
puts "min_max paths: [llength $mm_paths]"
