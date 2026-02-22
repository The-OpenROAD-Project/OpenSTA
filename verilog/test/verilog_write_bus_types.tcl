# Test write verilog bus design (exercises writeInstBusPin)

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
read_verilog verilog_bus_test.v
link_design verilog_bus_test

set out1 [make_result_file verilog_write_bus_types.v]
write_verilog $out1
assert_file_nonempty $out1
assert_file_contains $out1 "module verilog_bus_test"
assert_file_contains $out1 "data_in[0]"
assert_file_contains $out1 "data_out[3]"

set out2 [make_result_file verilog_write_bus_types_pwr.v]
write_verilog -include_pwr_gnd $out2
assert_file_nonempty $out2
assert_file_contains $out2 "module verilog_bus_test"
