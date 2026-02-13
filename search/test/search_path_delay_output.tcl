# Test ReportPath.cc: PathEndPathDelay and PathEndOutputDelay reporting,
# set_max_delay/set_min_delay path constraints,
# reportFull/reportShort/reportEndpoint for PathEndPathDelay,
# reportFull/reportShort/reportEndpoint for PathEndOutputDelay,
# report various formats for output delay paths,
# report_checks with -from/-to/-through combinations on path delay,
# checkRoleString for path delay endpoints.
# Targets: ReportPath.cc reportFull(PathEndPathDelay),
#          reportShort(PathEndPathDelay), reportEndpoint(PathEndPathDelay),
#          reportFull(PathEndOutputDelay), reportShort(PathEndOutputDelay),
#          reportEndpoint(PathEndOutputDelay), reportEndpointOutputDelay,
#          isPropagated, clkNetworkDelayIdealProp
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

############################################################
# Output delay paths with all formats
############################################################
puts "--- Output delay path formats ---"
report_checks -to [get_ports out1] -path_delay max -format full
report_checks -to [get_ports out1] -path_delay max -format full_clock
report_checks -to [get_ports out1] -path_delay max -format full_clock_expanded
report_checks -to [get_ports out1] -path_delay max -format short
report_checks -to [get_ports out1] -path_delay max -format end
report_checks -to [get_ports out1] -path_delay max -format summary
report_checks -to [get_ports out1] -path_delay max -format slack_only
report_checks -to [get_ports out1] -path_delay max -format json
puts "PASS: output delay formats"

puts "--- Output delay min formats ---"
report_checks -to [get_ports out1] -path_delay min -format full
report_checks -to [get_ports out1] -path_delay min -format full_clock
report_checks -to [get_ports out1] -path_delay min -format full_clock_expanded
report_checks -to [get_ports out1] -path_delay min -format short
report_checks -to [get_ports out1] -path_delay min -format end
report_checks -to [get_ports out1] -path_delay min -format json
puts "PASS: output delay min formats"

############################################################
# Output delay with fields
############################################################
puts "--- Output delay with fields ---"
report_checks -to [get_ports out1] -path_delay max -fields {capacitance slew fanout input_pin net src_attr}
report_checks -to [get_ports out1] -path_delay min -fields {capacitance slew fanout input_pin net src_attr}
puts "PASS: output delay fields"

############################################################
# set_max_delay: creates PathEndPathDelay
############################################################
puts "--- set_max_delay ---"
set_max_delay 5.0 -from [get_ports in1] -to [get_ports out1]
report_checks -path_delay max -format full
report_checks -path_delay max -format full_clock
report_checks -path_delay max -format full_clock_expanded
report_checks -path_delay max -format short
report_checks -path_delay max -format end
report_checks -path_delay max -format summary
report_checks -path_delay max -format json
report_checks -path_delay max -fields {capacitance slew fanout input_pin net}
puts "PASS: set_max_delay"

############################################################
# set_min_delay
############################################################
puts "--- set_min_delay ---"
set_min_delay 1.0 -from [get_ports in1] -to [get_ports out1]
report_checks -path_delay min -format full
report_checks -path_delay min -format full_clock
report_checks -path_delay min -format full_clock_expanded
report_checks -path_delay min -format short
report_checks -path_delay min -format end
report_checks -path_delay min -format json
report_checks -path_delay min -fields {capacitance slew fanout}
puts "PASS: set_min_delay"

############################################################
# find_timing_paths with path delay constraints
############################################################
puts "--- find_timing_paths with path delay ---"
set paths_pd [find_timing_paths -path_delay max -endpoint_path_count 10 -group_path_count 20]
puts "Max paths: [llength $paths_pd]"
foreach pe $paths_pd {
  puts "  is_path_delay: [$pe is_path_delay] is_output: [$pe is_output_delay] is_check: [$pe is_check] pin=[get_full_name [$pe pin]] slack=[$pe slack]"
}
puts "PASS: path delay paths"

puts "--- find_timing_paths min with path delay ---"
set paths_pd_min [find_timing_paths -path_delay min -endpoint_path_count 10]
puts "Min paths: [llength $paths_pd_min]"
foreach pe $paths_pd_min {
  puts "  is_path_delay: [$pe is_path_delay] is_output: [$pe is_output_delay] pin=[get_full_name [$pe pin]] slack=[$pe slack]"
}
puts "PASS: path delay min paths"

############################################################
# Remove path delay constraints
############################################################
puts "--- Remove path delay ---"
catch { reset_path -from [get_ports in1] -to [get_ports out1] }
report_checks -path_delay max
puts "PASS: remove path delay"

############################################################
# set_false_path
############################################################
puts "--- set_false_path ---"
set_false_path -from [get_ports in1] -to [get_ports out1]
report_checks -path_delay max
report_checks -path_delay max -unconstrained
puts "PASS: false_path"

catch { reset_path -from [get_ports in1] -to [get_ports out1] }
report_checks -path_delay max
puts "PASS: reset false_path"

############################################################
# set_multicycle_path
############################################################
puts "--- set_multicycle_path ---"
set_multicycle_path 2 -setup -from [get_ports in1] -to [get_ports out1]
report_checks -path_delay max -format full_clock_expanded
puts "PASS: multicycle setup"

set_multicycle_path 1 -hold -from [get_ports in1] -to [get_ports out1]
report_checks -path_delay min -format full_clock_expanded
puts "PASS: multicycle hold"

catch { reset_path -from [get_ports in1] -to [get_ports out1] }

############################################################
# Propagated clock with output delay
############################################################
puts "--- Propagated clock output delay ---"
set_propagated_clock [get_clocks clk]
report_checks -to [get_ports out1] -path_delay max -format full_clock_expanded -fields {capacitance slew}
report_checks -to [get_ports out1] -path_delay min -format full_clock_expanded -fields {capacitance slew}
unset_propagated_clock [get_clocks clk]
puts "PASS: propagated output delay"

############################################################
# Vary output delay and check slack
############################################################
puts "--- Output delay variation ---"
set_output_delay -clock clk 1.0 [get_ports out1]
set ws1 [sta::worst_slack_cmd max]
puts "output_delay 1.0: worst_slack=$ws1"

set_output_delay -clock clk 5.0 [get_ports out1]
set ws2 [sta::worst_slack_cmd max]
puts "output_delay 5.0: worst_slack=$ws2"

set_output_delay -clock clk 9.0 [get_ports out1]
set ws3 [sta::worst_slack_cmd max]
puts "output_delay 9.0: worst_slack=$ws3"

set_output_delay -clock clk 2.0 [get_ports out1]
puts "PASS: output delay variation"

############################################################
# report_checks with digits and no_line_splits
############################################################
puts "--- Output delay digits/no_split ---"
report_checks -to [get_ports out1] -path_delay max -digits 6 -format full_clock_expanded
report_checks -to [get_ports out1] -path_delay max -no_line_splits -format full_clock_expanded
puts "PASS: output digits/no_split"

############################################################
# PathEnd detailed iteration
############################################################
puts "--- PathEnd detail for output delay ---"
set pe_out [find_timing_paths -to [get_ports out1] -path_delay max]
foreach pe $pe_out {
  puts "is_output_delay: [$pe is_output_delay]"
  puts "is_check: [$pe is_check]"
  puts "is_path_delay: [$pe is_path_delay]"
  puts "margin: [$pe margin]"
  puts "data_arrival: [$pe data_arrival_time]"
  puts "data_required: [$pe data_required_time]"
  catch { puts "source_clk_offset: [$pe source_clk_offset]" }
  catch { puts "target_clk: [get_name [$pe target_clk]]" }
  catch { puts "target_clk_time: [$pe target_clk_time]" }
  break
}
puts "PASS: PathEnd output detail"

puts "ALL PASSED"
