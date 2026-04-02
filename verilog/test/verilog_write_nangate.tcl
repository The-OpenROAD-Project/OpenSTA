# Test write verilog with multiple cell types (Nangate45)

source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_test1.v
link_design verilog_test1

# Add various cell types to exercise more writer paths
set net_a [make_net wire_a]
set net_b [make_net wire_b]
set net_c [make_net wire_c]
set net_d [make_net wire_d]
set net_e [make_net wire_e]

# NAND gate
set inst_nand [make_instance nand1 NangateOpenCellLibrary/NAND2_X1]
connect_pin wire_a nand1/A1
connect_pin wire_b nand1/A2

# NOR gate
set inst_nor [make_instance nor1 NangateOpenCellLibrary/NOR2_X1]
connect_pin wire_c nor1/A1
connect_pin wire_d nor1/A2

# Another buffer with different drive
set inst_buf [make_instance buf_x4 NangateOpenCellLibrary/BUF_X4]
connect_pin wire_e buf_x4/A

puts "cells after additions: [llength [get_cells *]]"
puts "nets after additions: [llength [get_nets *]]"

# Write basic verilog
set out1 [make_result_file verilog_write_nangate_out1.v]
write_verilog $out1
diff_files $out1 verilog_write_nangate_out1.vok

# Write with pwr_gnd
set out2 [make_result_file verilog_write_nangate_out2.v]
write_verilog -include_pwr_gnd $out2
diff_files $out2 verilog_write_nangate_out2.vok

# Cleanup added instances/nets
disconnect_pin wire_a nand1/A1
disconnect_pin wire_b nand1/A2
disconnect_pin wire_c nor1/A1
disconnect_pin wire_d nor1/A2
disconnect_pin wire_e buf_x4/A
delete_instance nand1
delete_instance nor1
delete_instance buf_x4
delete_net wire_a
delete_net wire_b
delete_net wire_c
delete_net wire_d
delete_net wire_e
