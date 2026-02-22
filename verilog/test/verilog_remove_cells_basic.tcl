# Test 1: Write with -remove_cells option (nangate45 design)
# Test 4: Read back written file with removed cells
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

# Write with pwr_gnd and remove
set out_rm_pwr [make_result_file verilog_remove_pwr.v]
write_verilog -include_pwr_gnd -remove_cells {NangateOpenCellLibrary/BUF_X1} $out_rm_pwr

set sz_rm_pwr [file size $out_rm_pwr]
puts "remove+pwr size=$sz_rm_pwr"

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
