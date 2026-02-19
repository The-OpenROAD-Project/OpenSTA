# Test timing analysis commands
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

puts "--- report_tns max ---"
report_tns -max

puts "--- report_tns min ---"
report_tns -min

puts "--- report_wns max ---"
report_wns -max

puts "--- report_wns min ---"
report_wns -min

puts "--- report_worst_slack max ---"
report_worst_slack -max

puts "--- report_worst_slack min ---"
report_worst_slack -min

puts "--- check_setup verbose ---"
check_setup -verbose

puts "--- check_setup specific flags ---"
check_setup -verbose -no_input_delay -no_output_delay

puts "--- check_setup no_clock ---"
check_setup -verbose -no_clock

puts "--- check_setup unconstrained ---"
check_setup -verbose -unconstrained_endpoints

puts "--- check_setup loops ---"
check_setup -verbose -loops

puts "--- check_setup generated_clocks ---"
check_setup -verbose -generated_clocks

puts "--- report_check_types verbose ---"
report_check_types -verbose

puts "--- report_check_types max_delay ---"
report_check_types -max_delay

puts "--- report_check_types min_delay ---"
report_check_types -min_delay

puts "--- report_check_types max_slew ---"
report_check_types -max_slew

puts "--- report_check_types max_fanout ---"
report_check_types -max_fanout

puts "--- report_check_types max_capacitance ---"
report_check_types -max_capacitance

puts "--- report_check_types min_pulse_width ---"
report_check_types -min_pulse_width

puts "--- report_check_types min_period ---"
report_check_types -min_period

puts "--- report_check_types max_skew ---"
report_check_types -max_skew

puts "--- report_check_types violators ---"
report_check_types -violators

puts "--- report_clock_skew setup ---"
report_clock_skew -setup

puts "--- report_clock_skew hold ---"
report_clock_skew -hold

puts "--- report_clock_skew -include_internal_latency ---"
report_clock_skew -setup -include_internal_latency

puts "--- report_clock_latency ---"
report_clock_latency

puts "--- report_clock_latency -include_internal_latency ---"
report_clock_latency -include_internal_latency

puts "--- report_clock_min_period ---"
report_clock_min_period

puts "--- report_clock_min_period -include_port_paths ---"
report_clock_min_period -include_port_paths

puts "--- report_clock_properties ---"
report_clock_properties

puts "--- report_clock_properties clk ---"
report_clock_properties clk

puts "--- report_pulse_width_checks ---"
report_pulse_width_checks

puts "--- report_pulse_width_checks verbose ---"
report_pulse_width_checks -verbose

puts "--- report_pulse_width_checks on pin ---"
report_pulse_width_checks [get_pins reg1/CK]

puts "--- report_disabled_edges ---"
report_disabled_edges

puts "--- report_constant on instance ---"
report_constant [get_cells and1]

puts "--- report_constant on pin ---"
report_constant [get_pins and1/A1]

puts "--- find_timing_paths max ---"
set paths [find_timing_paths -path_delay max]
puts "Found [llength $paths] max paths"

puts "--- find_timing_paths min ---"
set paths_min [find_timing_paths -path_delay min]
puts "Found [llength $paths_min] min paths"

puts "--- find_timing_paths min_max ---"
set paths_mm [find_timing_paths -path_delay min_max]
puts "Found [llength $paths_mm] min_max paths"

puts "--- find_timing_paths with constraints ---"
set paths_from [find_timing_paths -from [get_ports in1] -path_delay max]
puts "Found [llength $paths_from] paths from in1"

puts "--- find_timing_paths -endpoint_path_count ---"
set paths_epc [find_timing_paths -endpoint_path_count 3]
puts "Found [llength $paths_epc] paths with endpoint_path_count 3"

puts "--- find_timing_paths -group_path_count ---"
set paths_gpc [find_timing_paths -group_path_count 5 -endpoint_path_count 5]
puts "Found [llength $paths_gpc] paths with group_path_count 5"

puts "--- find_timing_paths -sort_by_slack ---"
set paths_sorted [find_timing_paths -sort_by_slack -endpoint_path_count 3 -group_path_count 3]
puts "Found [llength $paths_sorted] sorted paths"

puts "--- find_timing_paths -unique_paths_to_endpoint ---"
set paths_unique [find_timing_paths -unique_paths_to_endpoint -endpoint_path_count 3 -group_path_count 3]
puts "Found [llength $paths_unique] unique paths"

puts "--- find_timing_paths -slack_max ---"
set paths_slkmax [find_timing_paths -slack_max 100]
puts "Found [llength $paths_slkmax] paths with slack_max 100"

puts "--- report_tns with digits ---"
report_tns -max -digits 6

puts "--- report_wns with digits ---"
report_wns -max -digits 6

puts "--- report_worst_slack with digits ---"
report_worst_slack -max -digits 6

puts "--- report_arrival ---"
report_arrival [get_pins reg1/Q]

puts "--- report_required ---"
report_required [get_pins reg1/D]

puts "--- report_slack ---"
report_slack [get_pins reg1/D]

puts "--- worst_slack hidden cmd ---"
set ws [worst_slack -max]
puts "Worst slack (max): $ws"

puts "--- worst_slack min ---"
set ws_min [worst_slack -min]
puts "Worst slack (min): $ws_min"

puts "--- total_negative_slack hidden cmd ---"
set tns_val [total_negative_slack -max]
puts "TNS (max): $tns_val"

puts "--- worst_negative_slack hidden cmd ---"
set wns_val [worst_negative_slack -max]
puts "WNS (max): $wns_val"
