# Test advanced verilog writer options
source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 1: Write verilog from ASAP7 design (has more complexity)
#---------------------------------------------------------------
puts "--- Test 1: ASAP7 write ---"
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz

read_verilog ../../test/reg1_asap7.v
link_design top

puts "cells: [llength [get_cells *]]"
puts "nets: [llength [get_nets *]]"
puts "ports: [llength [get_ports *]]"

# Write basic
set out1 [make_result_file verilog_advanced_out1.v]
write_verilog $out1

# Write with pwr_gnd
set out2 [make_result_file verilog_advanced_out2.v]
write_verilog -include_pwr_gnd $out2

# Write with remove_cells
set out3 [make_result_file verilog_advanced_out3.v]
write_verilog -remove_cells {} $out3

# Compare sizes
set sz1 [file size $out1]
set sz2 [file size $out2]
set sz3 [file size $out3]
puts "basic size: $sz1"
puts "pwr_gnd size: $sz2"
puts "remove_cells size: $sz3"

#---------------------------------------------------------------
# Test 2: Write after network modification
#---------------------------------------------------------------
puts "--- Test 2: Write after modification ---"

# Add an instance and net
set new_net [make_net extra_net]
set new_inst [make_instance extra_buf asap7sc7p5t_INVBUF_RVT/BUFx2_ASAP7_75t_R]
connect_pin extra_net extra_buf/A

set out4 [make_result_file verilog_advanced_out4.v]
write_verilog $out4

set sz4 [file size $out4]
puts "modified size: $sz4"
# Disconnect and delete
disconnect_pin extra_net extra_buf/A
delete_instance extra_buf
delete_net extra_net

#---------------------------------------------------------------
# Test 3: Write verilog for sky130 design with attributes
#---------------------------------------------------------------
puts "--- Test 3: Sky130 with attributes ---"
# Reset
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../test/verilog_attribute.v
link_design counter

set out5 [make_result_file verilog_advanced_out5.v]
write_verilog $out5

set out6 [make_result_file verilog_advanced_out6.v]
write_verilog -include_pwr_gnd $out6

set sz5 [file size $out5]
set sz6 [file size $out6]
puts "sky130 basic: $sz5, pwr_gnd: $sz6"

#---------------------------------------------------------------
# Test 4: Write verilog for Nangate45 design
#---------------------------------------------------------------
puts "--- Test 4: Nangate45 write ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_test1.v
link_design verilog_test1

set out7 [make_result_file verilog_advanced_out7.v]
write_verilog $out7

set out8 [make_result_file verilog_advanced_out8.v]
write_verilog -include_pwr_gnd $out8

set sz7 [file size $out7]
set sz8 [file size $out8]
puts "nangate45 basic: $sz7, pwr_gnd: $sz8"
