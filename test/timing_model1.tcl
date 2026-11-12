# write_timing_model clock passthrough writes only transitions with a path
source helpers.tcl
read_liberty ../examples/nangate45_typ.lib.gz
read_verilog timing_model1.v
link_design top
create_clock -name clk -period 10 [get_ports clk]
set_case_analysis rising [get_ports clk]
set model [make_result_file timing_model1.lib]
write_timing_model $model
report_file $model
