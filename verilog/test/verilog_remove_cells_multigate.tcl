# Test 2: Write with remove_cells for multi-gate design
source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 2: Write with remove_cells for multi-gate design
#---------------------------------------------------------------
puts "--- Test 2: remove_cells on multi-gate design ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog ../../dcalc/test/dcalc_multidriver_test.v
link_design dcalc_multidriver_test

set out_md_basic [make_result_file verilog_remove_md_basic.v]
write_verilog $out_md_basic

# Remove INV_X1
set out_md_inv [make_result_file verilog_remove_md_inv.v]
write_verilog -remove_cells {NangateOpenCellLibrary/INV_X1} $out_md_inv

# Remove AND2_X1
set out_md_and [make_result_file verilog_remove_md_and.v]
write_verilog -remove_cells {NangateOpenCellLibrary/AND2_X1} $out_md_and

# Remove NAND2_X1 and NOR2_X1
set out_md_gates [make_result_file verilog_remove_md_gates.v]
write_verilog -remove_cells {NangateOpenCellLibrary/NAND2_X1 NangateOpenCellLibrary/NOR2_X1} $out_md_gates

# Compare sizes
set sz_md [file size $out_md_basic]
set sz_md_inv [file size $out_md_inv]
set sz_md_and [file size $out_md_and]
set sz_md_gates [file size $out_md_gates]
puts "multigate sizes: basic=$sz_md inv=$sz_md_inv and=$sz_md_and gates=$sz_md_gates"
