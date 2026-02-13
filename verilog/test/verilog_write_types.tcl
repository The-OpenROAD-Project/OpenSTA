# Test write verilog with various cell types and connections
# Targets: VerilogWriter.cc uncovered functions:
#   writeInstBusPin (line 382, hit=0), writeInstBusPinBit (line 416, hit=0)
#   findPortNCcount (line 499, hit=0)
#   findHierChildren lambda (line 142, hit=0)
#   verilogPortDir for various directions (line 240)
#   writeAssigns (line 439)
#   findUnconnectedNetCount (line 469)
#   findChildNCcount (line 482)
# Also targets: VerilogReader.cc (various cell type reading paths)
#   VerilogParse.yy (parser paths for different constructs)

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 1: Write with multiple cell types (Nangate45)
#---------------------------------------------------------------
puts "--- Test 1: Write Nangate45 multi-type design ---"
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
set out1 [make_result_file verilog_types_out1.v]
write_verilog $out1
puts "PASS: write_verilog with multiple types"

if { [file exists $out1] && [file size $out1] > 0 } {
  puts "PASS: output file 1 exists, size: [file size $out1]"
}

# Write with pwr_gnd
set out2 [make_result_file verilog_types_out2.v]
write_verilog -include_pwr_gnd $out2
puts "PASS: write_verilog -include_pwr_gnd multi-type"

if { [file exists $out2] && [file size $out2] > 0 } {
  puts "PASS: output file 2 exists, size: [file size $out2]"
}

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
puts "PASS: cleanup"

#---------------------------------------------------------------
# Test 2: Write bus design (exercises writeInstBusPin)
#---------------------------------------------------------------
puts "--- Test 2: Write bus design ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_bus_test.v
link_design verilog_bus_test

set out3 [make_result_file verilog_types_bus.v]
write_verilog $out3
puts "PASS: write_verilog bus design"

if { [file exists $out3] && [file size $out3] > 0 } {
  puts "PASS: bus output exists, size: [file size $out3]"
}

set out4 [make_result_file verilog_types_bus_pwr.v]
write_verilog -include_pwr_gnd $out4
puts "PASS: write_verilog bus -include_pwr_gnd"

#---------------------------------------------------------------
# Test 3: Write assign design
#---------------------------------------------------------------
puts "--- Test 3: Write assign design ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_assign_test.v
link_design verilog_assign_test

set out5 [make_result_file verilog_types_assign.v]
write_verilog $out5
puts "PASS: write_verilog assign design"

if { [file exists $out5] && [file size $out5] > 0 } {
  puts "PASS: assign output exists, size: [file size $out5]"
}

#---------------------------------------------------------------
# Test 4: Write attribute design (sky130)
#---------------------------------------------------------------
puts "--- Test 4: Write attribute design ---"
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../test/verilog_attribute.v
link_design counter

set out6 [make_result_file verilog_types_attr.v]
write_verilog $out6
puts "PASS: write_verilog attribute design"

if { [file exists $out6] && [file size $out6] > 0 } {
  puts "PASS: attribute output exists, size: [file size $out6]"
}

set out7 [make_result_file verilog_types_attr_pwr.v]
write_verilog -include_pwr_gnd $out7
puts "PASS: write_verilog attribute -include_pwr_gnd"

if { [file exists $out7] && [file size $out7] > 0 } {
  puts "PASS: attr pwr output exists, size: [file size $out7]"
}

#---------------------------------------------------------------
# Test 5: Write ASAP7 design (different cell naming)
#---------------------------------------------------------------
puts "--- Test 5: Write ASAP7 design ---"
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz

read_verilog ../../test/reg1_asap7.v
link_design top

set out8 [make_result_file verilog_types_asap7.v]
write_verilog $out8
puts "PASS: write_verilog ASAP7"

if { [file exists $out8] && [file size $out8] > 0 } {
  puts "PASS: ASAP7 output exists, size: [file size $out8]"
}

set out9 [make_result_file verilog_types_asap7_pwr.v]
write_verilog -include_pwr_gnd $out9
puts "PASS: write_verilog ASAP7 -include_pwr_gnd"

# Write with remove_cells
set out10 [make_result_file verilog_types_asap7_remove.v]
write_verilog -remove_cells {} $out10
puts "PASS: write_verilog ASAP7 -remove_cells {}"

# Compare sizes
set sz8 [file size $out8]
set sz9 [file size $out9]
set sz10 [file size $out10]
puts "ASAP7 basic: $sz8, pwr_gnd: $sz9, remove_cells: $sz10"

#---------------------------------------------------------------
# Test 6: Write complex bus design
#---------------------------------------------------------------
puts "--- Test 6: Write complex bus design ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_complex_bus_test.v
link_design verilog_complex_bus_test

set out11 [make_result_file verilog_types_complex_bus.v]
write_verilog $out11
puts "PASS: write_verilog complex bus"

if { [file exists $out11] && [file size $out11] > 0 } {
  puts "PASS: complex bus output exists, size: [file size $out11]"
}

set out12 [make_result_file verilog_types_complex_bus_pwr.v]
write_verilog -include_pwr_gnd $out12
puts "PASS: write_verilog complex bus -include_pwr_gnd"

puts "ALL PASSED"
