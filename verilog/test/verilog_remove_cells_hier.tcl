# Test 7: Write hierarchical design with removes
# Exercises: findHierChildren, writeChild remove path
source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 7: Write hierarchical design with removes
# Exercises: findHierChildren, writeChild remove path
#---------------------------------------------------------------
puts "--- Test 7: hierarchical with removes ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog ../../network/test/network_hier_test.v
link_design network_hier_test

set out_h_rm [make_result_file verilog_remove_hier_buf.v]
write_verilog -remove_cells {NangateOpenCellLibrary/BUF_X1} $out_h_rm

set out_h_rm2 [make_result_file verilog_remove_hier_and.v]
write_verilog -remove_cells {NangateOpenCellLibrary/AND2_X1 NangateOpenCellLibrary/INV_X1} $out_h_rm2

set sz_h_rm [file size $out_h_rm]
set sz_h_rm2 [file size $out_h_rm2]
puts "hier remove sizes: buf=$sz_h_rm and_inv=$sz_h_rm2"

# Read back hierarchical with removes
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog $out_h_rm
link_design network_hier_test

set rt_h_cells [get_cells *]
puts "hier roundtrip cells: [llength $rt_h_cells]"

set rt_h_hier [get_cells -hierarchical *]
puts "hier roundtrip hier cells: [llength $rt_h_hier]"
