read_liberty one2one.lib

puts "TEST 1:"
read_verilog one2one_test1.v
link_design one2one_test1
create_clock -name clk -period 0
set_input_delay -clock clk 0 [all_inputs]
set_output_delay -clock clk 0 [all_outputs]
report_checks -group_count 5

puts "TEST 2:"
read_verilog one2one_test2.v
link_design one2one_test2
create_clock -name clk -period 0
set_input_delay -clock clk 0 [all_inputs]
set_output_delay -clock clk 0 [all_outputs]
report_checks -group_count 5
