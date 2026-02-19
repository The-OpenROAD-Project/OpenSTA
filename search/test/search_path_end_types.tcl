# Test PathEnd.cc variants - output_delay, recovery/removal, path_delay
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_path_end_types.v
link_design search_path_end_types

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_input_delay -clock clk 0.5 [get_ports rst]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]
set_output_delay -clock clk 2.0 [get_ports out3]

puts "--- Basic timing ---"
report_checks -path_delay max
report_checks -path_delay min

puts "--- output_delay paths ---"
report_checks -to [get_ports out1] -path_delay max -format full_clock_expanded
report_checks -to [get_ports out1] -path_delay min -format full_clock_expanded
report_checks -to [get_ports out2] -path_delay max -format full_clock_expanded
report_checks -to [get_ports out3] -path_delay max -format full_clock_expanded

puts "--- PathEnd with output delay - various formats ---"
report_checks -to [get_ports out1] -format full
report_checks -to [get_ports out1] -format full_clock
report_checks -to [get_ports out1] -format short
report_checks -to [get_ports out1] -format end
report_checks -to [get_ports out1] -format summary
report_checks -to [get_ports out1] -format slack_only
report_checks -to [get_ports out1] -format json

puts "--- Recovery/removal checks ---"
sta::set_recovery_removal_checks_enabled 1
report_checks -path_delay max
report_checks -path_delay min
report_check_types -verbose

puts "--- find_timing_paths with recovery/removal ---"
set paths [find_timing_paths -path_delay max -endpoint_path_count 10 -group_path_count 10]
puts "Found [llength $paths] paths"
foreach pe $paths {
  set role [$pe check_role]
  if { $role != "NULL" } {
    puts "  role=$role pin=[get_full_name [$pe pin]] slack=[$pe slack]"
  }
}

puts "--- PathEnd attribute queries ---"
set paths [find_timing_paths -path_delay max -endpoint_path_count 3]
foreach pe $paths {
  puts "  is_check: [$pe is_check]"
  puts "  is_output_delay: [$pe is_output_delay]"
  puts "  path_delay_margin_is_external: [$pe path_delay_margin_is_external]"
  set tclkp [$pe target_clk_path]
  if { $tclkp != "NULL" } {
    puts "  target_clk_path pin: [get_full_name [$tclkp pin]]"
  }
}

puts "--- set_max_delay to create path_delay PathEnd ---"
set_max_delay 3 -from [get_ports in1] -to [get_ports out1]
report_checks -path_delay max -from [get_ports in1] -to [get_ports out1] -format full_clock_expanded
set paths_pd [find_timing_paths -from [get_ports in1] -to [get_ports out1] -path_delay max]
foreach pe $paths_pd {
  puts "  is_path_delay: [$pe is_path_delay]"
  puts "  path_delay_margin_is_external: [$pe path_delay_margin_is_external]"
}
unset_path_exceptions -from [get_ports in1] -to [get_ports out1]

puts "--- Multiple output delays on same pin ---"
set_output_delay -clock clk -min 1.0 [get_ports out1]
set_output_delay -clock clk -max 3.0 [get_ports out1]
report_checks -to [get_ports out1] -path_delay max -format full_clock_expanded
report_checks -to [get_ports out1] -path_delay min -format full_clock_expanded

puts "--- report_checks with unconstrained paths ---"
report_checks -unconstrained -path_delay max
report_checks -unconstrained -path_delay min

puts "--- Detailed path with reg-to-reg ---"
report_checks -from [get_pins reg1/CK] -to [get_pins reg2/D] -path_delay max -format full_clock_expanded
report_checks -from [get_pins reg1/CK] -to [get_pins reg2/D] -path_delay min -format full_clock_expanded

sta::set_recovery_removal_checks_enabled 0
