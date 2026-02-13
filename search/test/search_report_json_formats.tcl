# Test JSON path reporting for all path end types and format full_clock_expanded
# for latch, output_delay, gated_clock, data_check paths.
# Also tests reportPathEnd dispatching for different formats and path types.
# Targets: ReportPath.cc reportJson, reportJsonHeader, reportJsonFooter,
#          reportPathEnd for json/full/full_clock/full_clock_expanded/shorter/
#          endpoint/summary/slack_only with different PathEnd subtypes,
#          reportSummaryLine, reportSlackOnly, reportEndLine,
#          PathEnd.cc PathEndLatchCheck, PathEndOutputDelay,
#          PathEndGatedClock, PathEndDataCheck - reportFull/reportShort,
#          margin, requiredTime, checkRole
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib

############################################################
# Part 1: Latch paths (PathEndLatchCheck)
############################################################
puts "=== Part 1: Latch paths ==="
read_verilog search_latch.v
link_design search_latch

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]

report_checks -path_delay max > /dev/null

puts "--- Latch report -format json ---"
report_checks -path_delay max -format json
report_checks -path_delay min -format json
puts "PASS: latch json"

puts "--- Latch report -format full_clock_expanded ---"
report_checks -path_delay max -format full_clock_expanded
report_checks -path_delay min -format full_clock_expanded
puts "PASS: latch full_clock_expanded"

puts "--- Latch report -format full_clock ---"
report_checks -path_delay max -format full_clock
report_checks -path_delay min -format full_clock
puts "PASS: latch full_clock"

puts "--- Latch report -format full ---"
report_checks -path_delay max -format full
puts "PASS: latch full"

puts "--- Latch report -format short ---"
report_checks -path_delay max -format short
puts "PASS: latch short"

puts "--- Latch report -format end ---"
report_checks -path_delay max -format end
report_checks -path_delay min -format end
puts "PASS: latch end"

puts "--- Latch report -format summary ---"
report_checks -path_delay max -format summary
report_checks -path_delay min -format summary
puts "PASS: latch summary"

puts "--- Latch report -format slack_only ---"
report_checks -path_delay max -format slack_only
report_checks -path_delay min -format slack_only
puts "PASS: latch slack_only"

puts "--- Latch find_timing_paths PathEnd queries ---"
set paths [find_timing_paths -path_delay max -endpoint_path_count 10]
puts "Found [llength $paths] max paths"
foreach pe $paths {
  puts "  check=[$pe is_check] latch=[$pe is_latch_check] out=[$pe is_output_delay] gated=[$pe is_gated_clock] data=[$pe is_data_check] unconst=[$pe is_unconstrained] role=[$pe check_role]"
  catch { puts "  margin=[$pe margin] req=[$pe data_required_time] arr=[$pe data_arrival_time]" }
  catch { puts "  target_clk=[get_name [$pe target_clk]] target_time=[$pe target_clk_time]" }
  catch { puts "  src_offset=[$pe source_clk_offset] tgt_offset=[$pe target_clk_offset]" }
  catch { puts "  target_clk_delay=[$pe target_clk_delay]" }
  catch { puts "  target_clk_insertion_delay=[$pe target_clk_insertion_delay]" }
  catch { puts "  target_clk_uncertainty=[$pe target_clk_uncertainty]" }
  catch { puts "  inter_clk_uncertainty=[$pe inter_clk_uncertainty]" }
  catch { puts "  check_crpr=[$pe check_crpr]" }
  catch { puts "  clk_skew=[$pe clk_skew]" }
}
puts "PASS: latch PathEnd queries"

puts "--- Latch report with fields ---"
report_checks -path_delay max -fields {capacitance slew fanout input_pin}
puts "PASS: latch fields"

puts "--- Latch to specific endpoints ---"
report_checks -to [get_ports out1] -path_delay max -format full_clock_expanded
report_checks -to [get_ports out2] -path_delay max -format full_clock_expanded
puts "PASS: latch to endpoints"

############################################################
# Part 2: Gated clock paths (PathEndGatedClock)
############################################################
puts "=== Part 2: Gated clock paths ==="
read_verilog search_gated_clk.v
link_design search_gated_clk

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 0.5 [get_ports en]
set_output_delay -clock clk 2.0 [get_ports out1]

sta::set_gated_clk_checks_enabled 1

report_checks -path_delay max > /dev/null

puts "--- Gated clock report -format json ---"
report_checks -path_delay max -format json
report_checks -path_delay min -format json
puts "PASS: gated json"

puts "--- Gated clock report -format full_clock_expanded ---"
report_checks -path_delay max -format full_clock_expanded
report_checks -path_delay min -format full_clock_expanded
puts "PASS: gated full_clock_expanded"

puts "--- Gated clock find_timing_paths ---"
set paths_g [find_timing_paths -path_delay max -endpoint_path_count 10 -group_path_count 20]
puts "Found [llength $paths_g] gated paths"
foreach pe $paths_g {
  puts "  gated=[$pe is_gated_clock] check=[$pe is_check] role=[$pe check_role] slack=[$pe slack]"
  catch { puts "  margin=[$pe margin]" }
  catch { puts "  target_clk_delay=[$pe target_clk_delay]" }
  catch { puts "  target_clk_insertion_delay=[$pe target_clk_insertion_delay]" }
}
puts "PASS: gated PathEnd queries"

puts "--- Gated clock report all formats ---"
report_checks -path_delay max -format full
report_checks -path_delay max -format short
report_checks -path_delay max -format end
report_checks -path_delay max -format summary
report_checks -path_delay max -format slack_only
puts "PASS: gated all formats"

sta::set_gated_clk_checks_enabled 0

############################################################
# Part 3: Output delay paths (PathEndOutputDelay)
############################################################
puts "=== Part 3: Output delay paths ==="
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 8.0 [get_ports out1]

report_checks -path_delay max > /dev/null

puts "--- Output delay report -format json ---"
report_checks -to [get_ports out1] -path_delay max -format json
report_checks -to [get_ports out1] -path_delay min -format json
puts "PASS: output delay json"

puts "--- Output delay report -format full_clock_expanded ---"
report_checks -to [get_ports out1] -path_delay max -format full_clock_expanded
report_checks -to [get_ports out1] -path_delay min -format full_clock_expanded
puts "PASS: output delay full_clock_expanded"

puts "--- Output delay find_timing_paths ---"
set paths_od [find_timing_paths -to [get_ports out1] -path_delay max -endpoint_path_count 5]
puts "Found [llength $paths_od] output delay paths"
foreach pe $paths_od {
  puts "  out=[$pe is_output_delay] check=[$pe is_check] role=[$pe check_role] slack=[$pe slack]"
  catch { puts "  margin=[$pe margin] req=[$pe data_required_time]" }
  catch { puts "  target_clk_delay=[$pe target_clk_delay]" }
  catch { puts "  target_clk_insertion_delay=[$pe target_clk_insertion_delay]" }
  catch { puts "  target_clk_uncertainty=[$pe target_clk_uncertainty]" }
}
puts "PASS: output delay PathEnd queries"

puts "--- Output delay report all formats ---"
report_checks -to [get_ports out1] -path_delay max -format full
report_checks -to [get_ports out1] -path_delay max -format full_clock
report_checks -to [get_ports out1] -path_delay max -format short
report_checks -to [get_ports out1] -path_delay max -format end
report_checks -to [get_ports out1] -path_delay max -format summary
report_checks -to [get_ports out1] -path_delay max -format slack_only
puts "PASS: output delay all formats"

############################################################
# Part 4: Data check paths (PathEndDataCheck)
############################################################
puts "=== Part 4: Data check paths ==="
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

# Set up data check constraints
catch {
  set_data_check -from [get_pins reg1/CK] -to [get_pins reg2/D] -setup 0.2
  set_data_check -from [get_pins reg1/CK] -to [get_pins reg2/D] -hold 0.1
}

report_checks -path_delay max > /dev/null

puts "--- Data check report -format json ---"
report_checks -path_delay max -format json
report_checks -path_delay min -format json
puts "PASS: data check json"

puts "--- Data check report -format full_clock_expanded ---"
report_checks -path_delay max -format full_clock_expanded
report_checks -path_delay min -format full_clock_expanded
puts "PASS: data check full_clock_expanded"

puts "--- Data check find_timing_paths ---"
set paths_dc [find_timing_paths -path_delay max -endpoint_path_count 15 -group_path_count 30]
puts "Found [llength $paths_dc] data check paths"
foreach pe $paths_dc {
  puts "  data=[$pe is_data_check] check=[$pe is_check] role=[$pe check_role] slack=[$pe slack]"
  catch { puts "  margin=[$pe margin]" }
  catch { puts "  target_clk_delay=[$pe target_clk_delay]" }
  catch { puts "  target_clk_uncertainty=[$pe target_clk_uncertainty]" }
}
puts "PASS: data check PathEnd queries"

puts "--- Data check report all formats ---"
report_checks -path_delay max -format full
report_checks -path_delay max -format full_clock
report_checks -path_delay max -format short
report_checks -path_delay max -format end
report_checks -path_delay max -format summary
report_checks -path_delay max -format slack_only
puts "PASS: data check all formats"

puts "--- Data check with -digits 6 ---"
report_checks -path_delay max -format full_clock_expanded -digits 6
puts "PASS: data check digits 6"

puts "--- Data check with -no_line_splits ---"
report_checks -path_delay max -no_line_splits
puts "PASS: data check no_line_splits"

############################################################
# Part 5: report_checks with -unconstrained for JSON
############################################################
puts "=== Part 5: Unconstrained JSON ==="
read_verilog search_test1.v
link_design search_test1

# No constraints - paths will be unconstrained
report_checks -unconstrained -format json
report_checks -unconstrained -format full_clock_expanded
report_checks -unconstrained -format summary
report_checks -unconstrained -format end
report_checks -unconstrained -format slack_only
puts "PASS: unconstrained all formats"

puts "ALL PASSED"
