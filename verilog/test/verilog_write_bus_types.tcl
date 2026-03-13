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

proc assert_files_equal {path_a path_b} {
  set in_a [open $path_a r]
  set text_a [read $in_a]
  close $in_a
  set in_b [open $path_b r]
  set text_b [read $in_b]
  close $in_b
  if {$text_a ne $text_b} {
    error "expected identical files: $path_a vs $path_b"
  }
}

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_bus_test.v
link_design verilog_bus_test

set out1 [make_result_file verilog_write_bus_types.v]
write_verilog $out1
assert_file_nonempty $out1
assert_files_equal verilog_bus_out.vok $out1
assert_file_contains $out1 {input [3:0] data_in;}
assert_file_contains $out1 {output [3:0] data_out;}
assert_file_contains $out1 {.A(data_in[0])}
assert_file_contains $out1 {.Q(data_out[3])}

set out2 [make_result_file verilog_write_bus_types_pwr.v]
write_verilog -include_pwr_gnd $out2
assert_file_nonempty $out2
assert_files_equal $out1 $out2
