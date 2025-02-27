# path_group_names

read_liberty asap7_small.lib.gz
read_verilog reg1_asap7.v
link_design top
create_clock -name clk -period 500 {clk1 clk2 clk3}
set_input_delay -clock clk 0 [all_inputs -no_clocks]
set_output_delay -clock clk 0 [all_outputs]
puts "Initial path groups: [sta::path_group_names]"
group_path -name In2Reg -from [all_inputs] -to [all_registers -data_pins]
group_path -name In2Out -from [all_inputs] -to [all_outputs]
group_path -name Reg2Out -from [all_registers -clock_pins] -to [all_outputs]
group_path -name Reg2Reg -from [all_registers -clock_pins] -to [all_registers -data_pins]
puts "Final path groups: [sta::path_group_names]"
