# Test write verilog bus design (exercises writeInstBusPin)

source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_bus_test.v
link_design verilog_bus_test

set out1 [make_result_file verilog_write_bus_types.v]
write_verilog $out1

set out2 [make_result_file verilog_write_bus_types_pwr.v]
write_verilog -include_pwr_gnd $out2
