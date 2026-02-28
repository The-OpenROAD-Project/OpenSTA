# Test write verilog attribute design (sky130)

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

read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../test/verilog_attribute.v
link_design counter

set out1 [make_result_file verilog_write_sky130_attr.v]
write_verilog $out1
assert_file_nonempty $out1
assert_file_contains $out1 "module counter"
assert_file_contains $out1 "sky130_fd_sc_hd__dfrtp_1"

set out2 [make_result_file verilog_write_sky130_attr_pwr.v]
write_verilog -include_pwr_gnd $out2
assert_file_nonempty $out2
assert_file_contains $out2 "module counter"
