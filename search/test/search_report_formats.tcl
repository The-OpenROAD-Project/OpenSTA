# Test report_checks with all format options
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

puts "--- report_checks -format full ---"
report_checks -format full

puts "--- report_checks -format full_clock ---"
report_checks -format full_clock

puts "--- report_checks -format full_clock_expanded ---"
report_checks -format full_clock_expanded

puts "--- report_checks -format short ---"
report_checks -format short

puts "--- report_checks -format end ---"
report_checks -format end

puts "--- report_checks -format slack_only ---"
report_checks -format slack_only

puts "--- report_checks -format summary ---"
report_checks -format summary

puts "--- report_checks -format json ---"
report_checks -format json

puts "--- report_checks -path_delay min ---"
report_checks -path_delay min

puts "--- report_checks -path_delay max ---"
report_checks -path_delay max

puts "--- report_checks -path_delay min_max ---"
report_checks -path_delay min_max

puts "--- report_checks -path_delay max_rise ---"
report_checks -path_delay max_rise

puts "--- report_checks -path_delay max_fall ---"
report_checks -path_delay max_fall

puts "--- report_checks -path_delay min_rise ---"
report_checks -path_delay min_rise

puts "--- report_checks -path_delay min_fall ---"
report_checks -path_delay min_fall

puts "--- report_checks -fields capacitance ---"
report_checks -fields {capacitance}

puts "--- report_checks -fields slew ---"
report_checks -fields {slew}

puts "--- report_checks -fields fanout ---"
report_checks -fields {fanout}

puts "--- report_checks -fields input_pin ---"
report_checks -fields {input_pin}

puts "--- report_checks -fields net ---"
report_checks -fields {net}

puts "--- report_checks -fields all ---"
report_checks -fields {capacitance slew fanout input_pin net}

puts "--- report_checks -from in1 ---"
report_checks -from [get_ports in1]

puts "--- report_checks -to out1 ---"
report_checks -to [get_ports out1]

puts "--- report_checks -through n1 ---"
report_checks -through [get_pins and1/ZN]

puts "--- report_checks -rise_from in1 ---"
report_checks -rise_from [get_ports in1]

puts "--- report_checks -fall_from in1 ---"
report_checks -fall_from [get_ports in1]

puts "--- report_checks -rise_to out1 ---"
report_checks -rise_to [get_ports out1]

puts "--- report_checks -fall_to out1 ---"
report_checks -fall_to [get_ports out1]

puts "--- report_checks -rise_through ---"
report_checks -rise_through [get_pins buf1/Z]

puts "--- report_checks -fall_through ---"
report_checks -fall_through [get_pins buf1/Z]

puts "--- report_checks -endpoint_count 3 ---"
report_checks -endpoint_count 3

puts "--- report_checks -group_path_count 2 ---"
report_checks -group_path_count 2

puts "--- report_checks -unique_paths_to_endpoint ---"
report_checks -endpoint_count 3 -unique_paths_to_endpoint

puts "--- report_checks -sort_by_slack ---"
report_checks -sort_by_slack

puts "--- report_checks -unconstrained ---"
report_checks -unconstrained

puts "--- report_checks -digits 6 ---"
report_checks -digits 6

puts "--- report_checks -no_line_splits ---"
report_checks -no_line_splits

puts "--- report_checks -slack_max 100 ---"
report_checks -slack_max 100

puts "--- report_checks -slack_min 0 ---"
report_checks -slack_min 0
