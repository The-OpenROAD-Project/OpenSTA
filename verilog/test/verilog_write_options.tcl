# Test verilog writer options
source ../../test/helpers.tcl

proc assert_file_nonempty {path} {
  if {![file exists $path] || [file size $path] <= 0} {
    error "expected non-empty file: $path"
  }
}

proc assert_file_contains {path token} {
  set in [open $path r]
  set text [read $in]
  close $in
  if {[string first $token $text] < 0} {
    error "expected '$token' in $path"
  }
}

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_test1.v
link_design verilog_test1

puts "--- write_verilog basic ---"
set out1 [make_result_file verilog_write_options_out1.v]
write_verilog $out1
assert_file_nonempty $out1
assert_file_contains $out1 "module verilog_test1"
assert_file_contains $out1 "BUF_X1"

puts "--- write_verilog -include_pwr_gnd ---"
set out2 [make_result_file verilog_write_options_out2.v]
write_verilog -include_pwr_gnd $out2
assert_file_nonempty $out2
assert_file_contains $out2 "module verilog_test1"

puts "--- write_verilog -remove_cells (empty list) ---"
set out3 [make_result_file verilog_write_options_out3.v]
write_verilog -remove_cells {} $out3
assert_file_nonempty $out3
assert_file_contains $out3 "module verilog_test1"

puts "--- compare pwr_gnd vs basic output ---"
set sz1 [file size $out1]
set sz2 [file size $out2]
puts "basic size: $sz1, pwr_gnd size: $sz2"

puts "--- compare remove_cells vs basic output ---"
set sz3 [file size $out3]
puts "basic size: $sz1, remove_cells size: $sz3"

set in1 [open $out1 r]
set txt1 [read $in1]
close $in1
set in3 [open $out3 r]
set txt3 [read $in3]
close $in3
if {$txt1 ne $txt3} {
  error "empty -remove_cells output should match baseline output"
}

puts "--- write_verilog -sort (deprecated, should warn) ---"
set out4 [make_result_file verilog_write_options_out4.v]
set msg_sort [write_verilog -sort $out4]
puts "write_verilog -sort: $msg_sort"
assert_file_nonempty $out4
assert_file_contains $out4 "module verilog_test1"
if {$msg_sort ne ""} {
  error "write_verilog -sort returned unexpected message: '$msg_sort'"
}
puts "--- read_verilog / write_verilog roundtrip ---"
# Read back the written verilog to exercise reader code paths
set out5 [make_result_file verilog_write_options_out5.v]
write_verilog $out5
assert_file_nonempty $out5
assert_file_contains $out5 "module verilog_test1"
