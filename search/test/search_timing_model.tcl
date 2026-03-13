# Test MakeTimingModel.cc - write_timing_model command
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

# Run timing first
report_checks -path_delay max > /dev/null

puts "--- write_timing_model ---"
set model_file [make_result_file "search_test1_model.lib"]
write_timing_model $model_file

puts "--- write_timing_model with cell_name ---"
set model_file2 [make_result_file "search_test1_model2.lib"]
write_timing_model -cell_name my_cell $model_file2

puts "--- write_timing_model with library_name ---"
set model_file3 [make_result_file "search_test1_model3.lib"]
write_timing_model -library_name my_lib -cell_name my_cell $model_file3
