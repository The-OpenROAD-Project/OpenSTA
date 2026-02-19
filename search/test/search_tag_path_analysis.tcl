# Test Tag.cc and Search.cc: tag operations during multi-clock timing analysis
# with exceptions, generated clocks, and multiple analysis corners.
# Tags are exercised when paths have different clocks, exception states,
# and segment start properties.
# Targets: Tag.cc Tag constructor, matchCrpr, isClock, isGenClkSrcPath,
#   isFilter, isLoop, isSegmentStart, tagMatchNoSense, findHash,
#   tagStateEqual, tagEqual, tagCmp, TagLess, TagHash, TagEqual,
#   Search.cc findTag, tagMatch, tagGroup, clkInfoTag, tagFree,
#   ClkInfo.cc various constructors and comparisons,
#   PathAnalysisPt.cc pathAPIndex construction
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog ../../sdc/test/sdc_test2.v
link_design sdc_test2

############################################################
# Phase 1: Two clocks - exercises tag creation for different clocks
############################################################
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 15 [get_ports clk2]

set_input_delay -clock clk1 1.0 [get_ports in1]
set_input_delay -clock clk1 1.0 [get_ports in2]
set_input_delay -clock clk2 2.0 [get_ports in3]
set_output_delay -clock clk1 2.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]

# Force timing analysis (creates tags)
report_checks -path_delay max
report_checks -path_delay min

############################################################
# Phase 2: Exception paths create additional tag states
############################################################
puts "--- exception state tags ---"
set_false_path -from [get_ports in1] -to [get_ports out2]
report_checks -path_delay max

set_multicycle_path 2 -setup -from [get_clocks clk1] -to [get_clocks clk2]
report_checks -path_delay max

set_max_delay 8.0 -from [get_ports in2] -through [get_pins and1/ZN] -to [get_ports out1]
report_checks -path_delay max

############################################################
# Phase 3: Group paths exercise tag matching
############################################################
puts "--- group_path tag matching ---"
group_path -name gp1 -from [get_ports in1]
group_path -name gp2 -to [get_ports out2]
report_checks -path_delay max -path_group gp1
report_checks -path_delay max -path_group gp2

############################################################
# Phase 4: find_timing_paths with many endpoints
# exercises tag comparison/sorting
############################################################
puts "--- find_timing_paths multi-clock ---"
set paths [find_timing_paths -path_delay max -endpoint_path_count 5 -group_path_count 20]
puts "multi-clock paths: [llength $paths]"
foreach pe $paths {
  puts "  [get_full_name [$pe pin]] slack=[$pe slack]"
}

puts "--- find_timing_paths min multi-clock ---"
set paths_min [find_timing_paths -path_delay min -endpoint_path_count 5 -group_path_count 20]
puts "min multi-clock: [llength $paths_min]"

############################################################
# Phase 5: Slack metrics exercise tag retrieval
############################################################
puts "--- total_negative_slack ---"
set tns_max [total_negative_slack -max]
puts "tns max: $tns_max"
set tns_min [total_negative_slack -min]
puts "tns min: $tns_min"

puts "--- worst_slack ---"
set ws_max [worst_slack -max]
puts "worst_slack max: $ws_max"
set ws_min [worst_slack -min]
puts "worst_slack min: $ws_min"

puts "--- worst_negative_slack ---"
set wns [worst_negative_slack -max]
puts "wns: $wns"

############################################################
# Phase 6: report_check_types exercises different check roles
# which use different tags
############################################################
puts "--- report_check_types all ---"
report_check_types -max_delay -min_delay -recovery -removal \
  -clock_gating_setup -clock_gating_hold \
  -min_pulse_width -min_period -max_skew \
  -max_slew -max_capacitance -max_fanout

############################################################
# Phase 7: Generated clock exercises gen clk src path tags
############################################################
puts "--- generated clock tags ---"
delete_clock [get_clocks clk2]
create_generated_clock -name gen_clk -source [get_ports clk1] \
  -divide_by 2 [get_ports clk2]

set_input_delay -clock gen_clk 1.5 [get_ports in3]
set_output_delay -clock gen_clk 2.5 [get_ports out2]

report_checks -path_delay max
report_checks -path_delay min

puts "--- report_checks -format full_clock ---"
report_checks -path_delay max -format full_clock

puts "--- report_checks -format full_clock_expanded ---"
report_checks -path_delay max -format full_clock_expanded

############################################################
# Phase 8: Clock uncertainty + latency create additional tag info
############################################################
puts "--- clock uncertainty + latency ---"
set_clock_latency -source 0.3 [get_clocks clk1]
set_clock_latency -source 0.5 [get_clocks gen_clk]
set_clock_uncertainty -setup 0.2 -from [get_clocks clk1] -to [get_clocks gen_clk]
set_clock_uncertainty -hold 0.1 -from [get_clocks clk1] -to [get_clocks gen_clk]

report_checks -path_delay max -format full_clock_expanded
report_checks -path_delay min -format full_clock_expanded

############################################################
# Phase 9: report_arrival / report_required / report_slack
# per-pin tag queries
############################################################
puts "--- per-pin tag queries ---"
report_arrival [get_pins reg1/D]
report_required [get_pins reg1/D]
report_slack [get_pins reg1/D]
report_arrival [get_pins reg3/D]
report_required [get_pins reg3/D]
report_slack [get_pins reg3/D]

############################################################
# Phase 10: report_clock_skew with generated clock
############################################################
puts "--- clock_skew with generated clock ---"
report_clock_skew -setup
report_clock_skew -hold

puts "--- report_clock_latency with generated ---"
report_clock_latency

puts "--- report_clock_min_period ---"
report_clock_min_period
