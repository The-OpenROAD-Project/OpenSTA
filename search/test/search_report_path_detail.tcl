# Test ReportPath.cc, PathEnum.cc, PathExpanded.cc, PathGroup.cc
# Exercises uncovered report path formats and path enumeration
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

puts "--- report_path on specific pin/transition ---"
report_path -max [get_pins reg1/D] rise
report_path -max [get_pins reg1/D] fall
report_path -min [get_pins reg1/D] rise
report_path -min [get_pins reg1/D] fall

puts "--- report_path on output pin ---"
report_path -max [get_ports out1] rise
report_path -max [get_ports out1] fall
report_path -min [get_ports out1] rise
report_path -min [get_ports out1] fall

puts "--- report_path with -all flag ---"
report_path -max -all [get_pins reg1/D] rise

puts "--- report_path with -tags flag ---"
report_path -max -tags [get_pins reg1/D] rise

puts "--- report_path with -all -tags combined ---"
report_path -max -all -tags [get_pins reg1/D] rise

puts "--- report_path with various formats ---"
report_path -max -format full [get_pins reg1/D] rise
report_path -max -format full_clock [get_pins reg1/D] rise
report_path -max -format full_clock_expanded [get_pins reg1/D] rise
report_path -max -format short [get_pins reg1/D] rise
report_path -max -format end [get_pins reg1/D] rise
report_path -max -format summary [get_pins reg1/D] rise
report_path -max -format slack_only [get_pins reg1/D] rise
report_path -max -format json [get_pins reg1/D] rise

puts "--- report_checks with -fields src_attr ---"
report_checks -fields {src_attr}

puts "--- report_checks with -fields all combined ---"
report_checks -fields {capacitance slew fanout input_pin net src_attr}

puts "--- PathEnd methods ---"
set path_ends [find_timing_paths -path_delay max -endpoint_path_count 3]
foreach pe $path_ends {
  puts "  is_unconstrained: [$pe is_unconstrained]"
  puts "  is_check: [$pe is_check]"
  puts "  is_latch_check: [$pe is_latch_check]"
  puts "  is_data_check: [$pe is_data_check]"
  puts "  is_output_delay: [$pe is_output_delay]"
  puts "  is_path_delay: [$pe is_path_delay]"
  puts "  is_gated_clock: [$pe is_gated_clock]"
  puts "  pin: [get_full_name [$pe pin]]"
  puts "  end_transition: [$pe end_transition]"
  puts "  slack: [$pe slack]"
  puts "  margin: [$pe margin]"
  puts "  data_required_time: [$pe data_required_time]"
  puts "  data_arrival_time: [$pe data_arrival_time]"
  puts "  check_role: [$pe check_role]"
  puts "  min_max: [$pe min_max]"
  puts "  source_clk_offset: [$pe source_clk_offset]"
  puts "  source_clk_latency: [$pe source_clk_latency]"
  puts "  source_clk_insertion_delay: [$pe source_clk_insertion_delay]"
  set tclk [$pe target_clk]
  if { $tclk != "NULL" } {
    puts "  target_clk: [get_name $tclk]"
  } else {
    puts "  target_clk: NULL"
  }
  set tclke [$pe target_clk_edge]
  puts "  target_clk_edge exists: [expr {$tclke != "NULL"}]"
  puts "  target_clk_time: [$pe target_clk_time]"
  puts "  target_clk_offset: [$pe target_clk_offset]"
  puts "  target_clk_mcp_adjustment: [$pe target_clk_mcp_adjustment]"
  puts "  target_clk_delay: [$pe target_clk_delay]"
  puts "  target_clk_insertion_delay: [$pe target_clk_insertion_delay]"
  puts "  target_clk_uncertainty: [$pe target_clk_uncertainty]"
  puts "  inter_clk_uncertainty: [$pe inter_clk_uncertainty]"
  puts "  target_clk_arrival: [$pe target_clk_arrival]"
  puts "  check_crpr: [$pe check_crpr]"
  puts "  target_clk_end_trans: [$pe target_clk_end_trans]"
  puts "  clk_skew: [$pe clk_skew]"
  break
}

puts "--- Path methods ---"
foreach pe $path_ends {
  set p [$pe path]
  puts "  arrival: [$p arrival]"
  puts "  required: [$p required]"
  puts "  slack: [$p slack]"
  puts "  pin: [get_full_name [$p pin]]"
  puts "  edge: [$p edge]"
  puts "  tag: [$p tag]"
  set pins [$p pins]
  puts "  path pins: [llength $pins]"
  set sp [$p start_path]
  if { $sp != "NULL" } {
    puts "  start_path pin: [get_full_name [$sp pin]]"
  }
  break
}

puts "--- group_path with various options ---"
group_path -name reg_group -from [get_ports in1]
group_path -name out_group -to [get_ports out1]
report_checks -path_delay max

puts "--- find_timing_paths with path groups ---"
set paths [find_timing_paths -sort_by_slack -group_path_count 5 -endpoint_path_count 3]
puts "Found [llength $paths] paths"

puts "--- find_timing_paths unique paths ---"
set upaths [find_timing_paths -unique_paths_to_endpoint -endpoint_path_count 5 -group_path_count 5]
puts "Found [llength $upaths] unique paths"

puts "--- Search internal commands ---"
puts "tag_group_count: [sta::tag_group_count]"
puts "tag_count: [sta::tag_count]"
puts "clk_info_count: [sta::clk_info_count]"
puts "path_count: [sta::path_count]"
puts "endpoint_violation_count max: [sta::endpoint_violation_count max]"
puts "endpoint_violation_count min: [sta::endpoint_violation_count min]"

puts "--- Startpoints and endpoints ---"
set starts [sta::startpoints]
puts "Startpoints: [llength $starts]"
set ends [sta::endpoints]
puts "Endpoints: [llength $ends]"
puts "Endpoint count: [sta::endpoint_path_count]"

puts "--- Path group names ---"
set group_names [sta::path_group_names]
puts "Path group names: $group_names"

puts "--- Endpoint slack ---"
set pin [get_pins reg1/D]
# catch: sta::endpoint_slack may fail if path group "reg_to_reg" does not exist
catch {
  set eslack [sta::endpoint_slack $pin "reg_to_reg" max]
  puts "Endpoint slack: $eslack"
}

puts "--- find_requireds ---"
sta::find_requireds

puts "--- report internal debug ---"
sta::report_tag_groups
sta::report_tags
sta::report_clk_infos
sta::report_arrival_entries
sta::report_required_entries
sta::report_path_count_histogram

puts "--- report_path_end header/footer ---"
set pe_for_report [find_timing_paths -path_delay max -endpoint_path_count 1]
sta::report_path_end_header
foreach pe $pe_for_report {
  sta::report_path_end $pe
}
sta::report_path_end_footer

puts "--- slow_drivers ---"
set slow [sta::slow_drivers 3]
puts "Slow drivers: [llength $slow]"

puts "--- levelize ---"
sta::levelize
