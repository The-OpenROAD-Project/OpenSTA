# Test write verilog attribute design (sky130)

source ../../test/helpers.tcl

read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../test/verilog_attribute.v
link_design counter

set out1 [make_result_file verilog_write_sky130_attr.v]
write_verilog $out1

set out2 [make_result_file verilog_write_sky130_attr_pwr.v]
write_verilog -include_pwr_gnd $out2
