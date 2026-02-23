# Test 5: Write and re-read complex bus design with removes
source ../../test/helpers.tcl

proc assert_file_nonempty {path} {
  if {![file exists $path]} {
    error "expected non-empty file: $path"
  }
  set in [open $path r]
  set text [read $in]
  close $in
  if {[string length $text] <= 0} {
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

proc assert_file_not_contains {path token} {
  set in [open $path r]
  set text [read $in]
  close $in
  if {[string first $token $text] >= 0} {
    error "did not expect '$token' in $path"
  }
}

#---------------------------------------------------------------
# Test 5: Write and re-read complex bus design with removes
#---------------------------------------------------------------
puts "--- Test 5: complex bus with removes ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_complex_bus_test.v
link_design verilog_complex_bus_test

set out_cb_rm [make_result_file verilog_remove_complex_buf.v]
write_verilog -remove_cells {BUF_X1} $out_cb_rm
assert_file_nonempty $out_cb_rm
assert_file_contains $out_cb_rm "module verilog_complex_bus_test"
assert_file_not_contains $out_cb_rm " BUF_X1 "

set out_cb_rm2 [make_result_file verilog_remove_complex_dff.v]
write_verilog -remove_cells {DFF_X1} $out_cb_rm2
assert_file_nonempty $out_cb_rm2
assert_file_contains $out_cb_rm2 "module verilog_complex_bus_test"
assert_file_not_contains $out_cb_rm2 " DFF_X1 "

diff_files verilog_remove_complex_buf.vok $out_cb_rm
diff_files verilog_remove_complex_dff.vok $out_cb_rm2

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog $out_cb_rm
link_design verilog_complex_bus_test
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {data_a[*]}]
set_input_delay -clock clk 0 [get_ports {data_b[*]}]
set_output_delay -clock clk 0 [get_ports {result[*]}]
set_output_delay -clock clk 0 [get_ports carry]
set_output_delay -clock clk 0 [get_ports overflow]
set_input_transition 0.1 [all_inputs]
with_output_to_variable rm_rep { report_checks -path_delay max }
if {![regexp {Path Type:\s+max} $rm_rep]} {
  error "remove_cells complex roundtrip timing report missing max path"
}
