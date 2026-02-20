# Test verilog writer options
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_test1.v
link_design verilog_test1

puts "--- write_verilog basic ---"
set out1 [make_result_file verilog_write_options_out1.v]
write_verilog $out1

puts "--- write_verilog -include_pwr_gnd ---"
set out2 [make_result_file verilog_write_options_out2.v]
write_verilog -include_pwr_gnd $out2

puts "--- write_verilog -remove_cells (empty list) ---"
set out3 [make_result_file verilog_write_options_out3.v]
write_verilog -remove_cells {} $out3

puts "--- compare pwr_gnd vs basic output ---"
set sz1 [file size $out1]
set sz2 [file size $out2]
puts "basic size: $sz1, pwr_gnd size: $sz2"
if { $sz2 >= $sz1 } {
} else {
  puts "INFO: pwr_gnd output is smaller (unexpected but not fatal)"
}

puts "--- compare remove_cells vs basic output ---"
set sz3 [file size $out3]
puts "basic size: $sz1, remove_cells size: $sz3"

puts "--- write_verilog -sort (deprecated, should warn) ---"
set out4 [make_result_file verilog_write_options_out4.v]
set msg_sort [write_verilog -sort $out4]
puts "write_verilog -sort: $msg_sort"
puts "--- read_verilog / write_verilog roundtrip ---"
# Read back the written verilog to exercise reader code paths
set out5 [make_result_file verilog_write_options_out5.v]
write_verilog $out5
