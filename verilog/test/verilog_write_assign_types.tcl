# Test write verilog assign design

source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_assign_test.v
link_design verilog_assign_test

set out1 [make_result_file verilog_write_assign_types.v]
write_verilog $out1
