read_liberty liberty_arcs_one2one.lib

puts "TEST 1:"
read_verilog liberty_arcs_one2one_1.v
link_design liberty_arcs_one2one_1
create_clock -name clk -period 0
set_input_delay -clock clk 0 [all_inputs]
set_output_delay -clock clk 0 [all_outputs]
report_checks -group_count 5

puts "TEST 2:"
read_verilog liberty_arcs_one2one_2.v
link_design liberty_arcs_one2one_2
create_clock -name clk -period 0
set_input_delay -clock clk 0 [all_inputs]
set_output_delay -clock clk 0 [all_outputs]
report_checks -group_count 5
