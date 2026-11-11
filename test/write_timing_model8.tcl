# write_timing_model input -> output combinational arcs.
# OpenROAD issue #11752.
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog write_timing_model8.v
link_design top
create_clock -name clk -period 10 [get_ports clk]

set model_file [make_result_file write_timing_model8.lib]
write_timing_model -library_name io_lib -cell_name io_cell $model_file
report_file $model_file
