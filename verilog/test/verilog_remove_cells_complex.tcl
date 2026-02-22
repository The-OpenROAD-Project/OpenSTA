# Test 5: Write and re-read complex bus design with removes
source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 5: Write and re-read complex bus design with removes
#---------------------------------------------------------------
puts "--- Test 5: complex bus with removes ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_complex_bus_test.v
link_design verilog_complex_bus_test

set out_cb_rm [make_result_file verilog_remove_complex_buf.v]
write_verilog -remove_cells {NangateOpenCellLibrary/BUF_X1} $out_cb_rm

set out_cb_rm2 [make_result_file verilog_remove_complex_dff.v]
write_verilog -remove_cells {NangateOpenCellLibrary/DFF_X1} $out_cb_rm2

set sz_cb_rm1 [file size $out_cb_rm]
set sz_cb_rm2 [file size $out_cb_rm2]
puts "complex remove sizes: buf=$sz_cb_rm1 dff=$sz_cb_rm2"
