# Test advanced verilog writer options - ASAP7 design
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
