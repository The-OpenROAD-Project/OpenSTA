# Test ReportPath.cc: latch path reporting with full_clock_expanded format,
# reportFull/reportShort/reportEndpoint for PathEndLatchCheck,
# reportBorrowing, latchDesc, report with fields {capacitance slew input_pin},
# report_path_end with latch PathEnd chaining,
# also report_checks with various digits and -no_line_splits on latch paths.
# Targets: ReportPath.cc reportFull(PathEndLatchCheck), reportShort(PathEndLatchCheck),
#          reportEndpoint(PathEndLatchCheck), reportBorrowing, latchDesc,
#          reportSrcClkAndPath, reportTgtClk for latch paths,
#          reportPathLine with fields, reportPath5/6 with all fields.
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_latch.v
link_design search_latch

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]

############################################################
# Latch path full_clock_expanded with all fields
############################################################
puts "--- Latch full_clock_expanded max with all fields ---"
report_checks -path_delay max -format full_clock_expanded -fields {capacitance slew fanout input_pin net src_attr}

puts "--- Latch full_clock_expanded min with all fields ---"
report_checks -path_delay min -format full_clock_expanded -fields {capacitance slew fanout input_pin net src_attr}

############################################################
# Latch path per endpoint
############################################################
puts "--- Latch report to latch output ---"
report_checks -to [get_ports out1] -path_delay max -format full_clock_expanded -fields {capacitance slew fanout}
report_checks -to [get_ports out1] -path_delay min -format full_clock_expanded -fields {capacitance slew fanout}

puts "--- Latch report to reg output ---"
report_checks -to [get_ports out2] -path_delay max -format full_clock_expanded -fields {capacitance slew input_pin}
report_checks -to [get_ports out2] -path_delay min -format full_clock_expanded -fields {capacitance slew input_pin}

############################################################
# Latch path report with digits
############################################################
puts "--- Latch full_clock_expanded digits 6 ---"
report_checks -path_delay max -format full_clock_expanded -digits 6
report_checks -path_delay min -format full_clock_expanded -digits 6

############################################################
# Latch path report with no_line_splits
############################################################
puts "--- Latch full_clock_expanded no_line_splits ---"
report_checks -path_delay max -format full_clock_expanded -no_line_splits
report_checks -path_delay min -format full_clock_expanded -no_line_splits

############################################################
# Latch path full_clock format
############################################################
puts "--- Latch full_clock format ---"
report_checks -path_delay max -format full_clock -fields {capacitance slew fanout input_pin net}
report_checks -path_delay min -format full_clock -fields {capacitance slew fanout input_pin net}

############################################################
# find_timing_paths and iterate latch paths
############################################################
puts "--- find_timing_paths latch iteration ---"
set paths_max [find_timing_paths -path_delay max -endpoint_path_count 10 -group_path_count 20]
puts "Max paths: [llength $paths_max]"
foreach pe $paths_max {
  puts "  is_latch: [$pe is_latch_check] is_check: [$pe is_check] slack=[$pe slack]"
  puts "  data_arrival: [$pe data_arrival_time] data_required: [$pe data_required_time]"
  puts "  margin: [$pe margin]"
  puts "  source_clk_latency: [$pe source_clk_latency]"
  puts "  target_clk_delay: [$pe target_clk_delay]"
  puts "  target_clk_uncertainty: [$pe target_clk_uncertainty]"
}

############################################################
# report_path_ends for latch paths
############################################################
puts "--- report_path_ends for latch paths ---"
sta::report_path_ends $paths_max

############################################################
# Latch report in end/summary/slack_only
############################################################
puts "--- Latch end format ---"
report_checks -path_delay max -format end
report_checks -path_delay min -format end

puts "--- Latch summary format ---"
report_checks -path_delay max -format summary
report_checks -path_delay min -format summary

puts "--- Latch slack_only format ---"
report_checks -path_delay max -format slack_only
report_checks -path_delay min -format slack_only

############################################################
# set_latch_borrow_limit and report with fields
############################################################
puts "--- set_latch_borrow_limit and report ---"
catch {
  set_latch_borrow_limit 2.5 [get_pins latch1/G]
  report_checks -path_delay max -format full_clock_expanded -fields {capacitance slew fanout input_pin}
}

############################################################
# report_checks min_max for latch
############################################################
puts "--- Latch min_max ---"
report_checks -path_delay min_max -format full_clock_expanded

############################################################
# PathEnd properties for latch paths
############################################################
puts "--- Latch PathEnd properties ---"
set paths_max2 [find_timing_paths -path_delay max -endpoint_path_count 10 -group_path_count 20]
foreach pe $paths_max2 {
  if { [$pe is_latch_check] } {
    puts "Latch path found:"
    set sp [get_property $pe startpoint]
    puts "  startpoint: [get_full_name $sp]"
    set ep [get_property $pe endpoint]
    puts "  endpoint: [get_full_name $ep]"
    puts "  slack: [get_property $pe slack]"
    set ec [get_property $pe endpoint_clock]
    puts "  endpoint_clock: [get_name $ec]"
    set ecp [get_property $pe endpoint_clock_pin]
    puts "  endpoint_clock_pin: [get_full_name $ecp]"
    set points [get_property $pe points]
    puts "  points: [llength $points]"
    break
  }
}

############################################################
# report_path_ends for latch paths
############################################################
puts "--- report_path_ends latch ---"
sta::report_path_ends $paths_max2

############################################################
# Latch JSON format (must be last, sets internal json state)
############################################################
puts "--- Latch JSON format ---"
report_checks -path_delay max -format json
report_checks -path_delay min -format json
