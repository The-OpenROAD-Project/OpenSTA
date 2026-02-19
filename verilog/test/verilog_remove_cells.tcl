# Test verilog writer -remove_cells option and re-read operations.
# Targets:
#   VerilogWriter.cc: writeChild with remove_cells filtering,
#     findChildNCcount with remove_cells skip, writeChildren sorted output,
#     writeInstBusPin/writeInstBusPinBit, findUnconnectedNetCount,
#     findPortNCcount, writeAssigns
#   VerilogReader.cc: multiple read_verilog calls (deleteModules paths),
#     read with missing cells (black box generation),
#     bus declaration parsing, link_design various paths,
#     VerilogNetConcat, VerilogNetPartSelect, module re-definition
#   VerilogNamespace.cc: cellVerilogName, instanceVerilogName,
#     netVerilogName, portVerilogName for various names

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 1: Write with -remove_cells option (nangate45 design)
#---------------------------------------------------------------
puts "--- Test 1: write with -remove_cells ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_test1.v
link_design verilog_test1

set cells [get_cells *]
puts "cells: [llength $cells]"

# Write without remove
set out_basic [make_result_file verilog_remove_basic.v]
write_verilog $out_basic

# Write with empty remove_cells list
set out_empty [make_result_file verilog_remove_empty.v]
write_verilog -remove_cells {} $out_empty

set sz_basic [file size $out_basic]
set sz_empty [file size $out_empty]
puts "basic size=$sz_basic empty remove size=$sz_empty"

# Write with specific cells to remove (BUF_X1)
set out_rm_buf [make_result_file verilog_remove_buf.v]
write_verilog -remove_cells {NangateOpenCellLibrary/BUF_X1} $out_rm_buf

set sz_rm_buf [file size $out_rm_buf]
puts "remove BUF_X1 size=$sz_rm_buf"

if { $sz_rm_buf < $sz_basic } {
}

# Write with DFF_X1 removed
set out_rm_dff [make_result_file verilog_remove_dff.v]
write_verilog -remove_cells {NangateOpenCellLibrary/DFF_X1} $out_rm_dff

set sz_rm_dff [file size $out_rm_dff]
puts "remove DFF_X1 size=$sz_rm_dff"

# Write with both removed
set out_rm_both [make_result_file verilog_remove_both.v]
write_verilog -remove_cells {NangateOpenCellLibrary/BUF_X1 NangateOpenCellLibrary/DFF_X1} $out_rm_both

set sz_rm_both [file size $out_rm_both]
puts "remove both size=$sz_rm_both"

if { $sz_rm_both <= $sz_rm_buf && $sz_rm_both <= $sz_rm_dff } {
}

# Write with pwr_gnd and remove
set out_rm_pwr [make_result_file verilog_remove_pwr.v]
write_verilog -include_pwr_gnd -remove_cells {NangateOpenCellLibrary/BUF_X1} $out_rm_pwr

set sz_rm_pwr [file size $out_rm_pwr]
puts "remove+pwr size=$sz_rm_pwr"

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

#---------------------------------------------------------------
# Test 3: Multiple re-reads of same file
# Exercises: module re-definition paths in VerilogReader
#---------------------------------------------------------------
puts "--- Test 3: multiple re-reads ---"

# Read same file multiple times
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_test1.v
read_verilog verilog_test1.v
link_design verilog_test1

set cells_rr [get_cells *]
puts "re-read cells: [llength $cells_rr]"

set nets_rr [get_nets *]
puts "re-read nets: [llength $nets_rr]"

# Read different file then same file
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_bus_test.v
read_verilog verilog_test1.v
link_design verilog_test1

set cells_rr2 [get_cells *]
puts "re-read2 cells: [llength $cells_rr2]"

# Read same bus file multiple times
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_bus_test.v
read_verilog verilog_bus_test.v
read_verilog verilog_bus_test.v
link_design verilog_bus_test

set cells_rr3 [get_cells *]
puts "re-read3 bus cells: [llength $cells_rr3]"

#---------------------------------------------------------------
# Test 4: Read back written file with removed cells
# Exercises: link_design with make_black_boxes when cells missing
#---------------------------------------------------------------
puts "--- Test 4: read back removed cells ---"

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog $out_rm_buf
link_design verilog_test1
set rt_cells [get_cells *]
puts "roundtrip (buf removed) cells: [llength $rt_cells]"

# Read back with all libs (should link normally)
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog $out_basic
link_design verilog_test1

set rt2_cells [get_cells *]
puts "roundtrip basic cells: [llength $rt2_cells]"

# Timing on roundtrip
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_output_delay -clock clk 0 [get_ports out1]
set_input_transition 0.1 [all_inputs]
report_checks

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

#---------------------------------------------------------------
# Test 6: Write assign/tristate design with removes
#---------------------------------------------------------------
puts "--- Test 6: supply/tristate with removes ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_supply_tristate.v
link_design verilog_supply_tristate

set out_st_rm [make_result_file verilog_remove_supply_buf.v]
write_verilog -remove_cells {NangateOpenCellLibrary/BUF_X1} $out_st_rm

set out_st_pwr [make_result_file verilog_remove_supply_pwr.v]
write_verilog -include_pwr_gnd -remove_cells {NangateOpenCellLibrary/INV_X1} $out_st_pwr

# Sizes
set sz_st_rm [file size $out_st_rm]
set sz_st_pwr [file size $out_st_pwr]
puts "supply remove sizes: buf=$sz_st_rm inv_pwr=$sz_st_pwr"

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
