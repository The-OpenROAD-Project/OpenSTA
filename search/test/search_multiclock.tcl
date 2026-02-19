# Test multi-clock designs, ClkNetwork.cc, PathGroup.cc, VisitPathEnds.cc
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_crpr.v
link_design search_crpr

# Create two clocks on same port (useful edge case)
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

puts "--- Basic multi-reg timing ---"
report_checks -path_delay max
report_checks -path_delay min

puts "--- find_timing_paths with various filters ---"
set paths_max [find_timing_paths -path_delay max -sort_by_slack -endpoint_path_count 5 -group_path_count 10]
puts "Max paths: [llength $paths_max]"
foreach pe $paths_max {
  puts "  [get_full_name [$pe pin]] slack=[$pe slack]"
}

set paths_min [find_timing_paths -path_delay min -sort_by_slack -endpoint_path_count 5 -group_path_count 10]
puts "Min paths: [llength $paths_min]"
foreach pe $paths_min {
  puts "  [get_full_name [$pe pin]] slack=[$pe slack]"
}

puts "--- find_timing_paths with slack_min/slack_max ---"
set paths_slack [find_timing_paths -path_delay max -slack_max 100 -slack_min -100]
puts "Paths with slack filter: [llength $paths_slack]"

puts "--- group_path ---"
group_path -name input_group -from [get_ports {in1 in2}]
group_path -name output_group -to [get_ports out1]
group_path -name reg2reg -from [get_pins reg1/CK] -to [get_pins reg2/D]
report_checks -path_delay max
report_checks -path_delay min

puts "--- group_path with -weight ---"
catch { group_path -name weighted_group -from [get_ports in1] -weight 2.0 }

puts "--- group_path with -default ---"
catch { group_path -name default_group -default }

puts "--- report_checks with -group filter ---"
catch {
  report_checks -path_delay max -group_path_count 3
}

puts "--- report_path_end with specific endpoints ---"
set pe_list [find_timing_paths -path_delay max -endpoint_path_count 3]
foreach pe $pe_list {
  sta::report_path_end $pe
}

puts "--- report_path_ends ---"
sta::report_path_ends $pe_list

puts "--- PathEnd attributes on different types ---"
foreach pe $pe_list {
  set v [$pe vertex]
  puts "  vertex pin: [get_full_name [$v pin]]"
  set p [$pe path]
  puts "  path arrival: [$p arrival]"
  puts "  path required: [$p required]"
}

puts "--- find_timing_paths min_max ---"
set paths_mm [find_timing_paths -path_delay min_max -endpoint_path_count 3]
puts "Min_max paths: [llength $paths_mm]"

puts "--- find_timing_paths with unique_edges ---"
catch {
  set paths_ue [find_timing_paths -path_delay max -endpoint_path_count 5 -unique_paths_to_endpoint]
  puts "Unique edge paths: [llength $paths_ue]"
}

puts "--- set_clock_sense ---"
catch { set_clock_sense -positive [get_pins ckbuf1/Z] -clocks clk }
report_checks -path_delay max
catch { set_clock_sense -stop [get_pins ckbuf2/Z] -clocks clk }
report_checks -path_delay max

puts "--- report_check_types ---"
report_check_types -max_delay -verbose
report_check_types -min_delay -verbose
