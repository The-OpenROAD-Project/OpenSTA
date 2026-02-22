# Test advanced verilog writer options - Sky130 with attributes
source ../../test/helpers.tcl

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
