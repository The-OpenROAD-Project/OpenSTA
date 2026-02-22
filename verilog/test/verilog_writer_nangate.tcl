# Test advanced verilog writer options - Nangate45 write
source ../../test/helpers.tcl

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
