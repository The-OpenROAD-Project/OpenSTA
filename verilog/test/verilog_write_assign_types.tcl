# Test write verilog assign design

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
read_verilog verilog_assign_test.v
link_design verilog_assign_test

set out1 [make_result_file verilog_write_assign_types.v]
write_verilog $out1
assert_file_nonempty $out1
assert_file_contains $out1 "module verilog_assign_test"
assert_file_contains $out1 ".A2(in3)"
assert_file_contains $out1 ".D(in3)"
