# Test 2: Write with remove_cells for multi-gate design
source ../../test/helpers.tcl
suppress_msg 1140

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

proc assert_file_not_has_cell {path cell_name} {
  set in [open $path r]
  set text [read $in]
  close $in
  set cell_pat [format {(^|[^A-Za-z0-9_])%s([^A-Za-z0-9_]|$)} $cell_name]
  if {[regexp -- $cell_pat $text]} {
    error "did not expect cell '$cell_name' in $path"
  }
}

#---------------------------------------------------------------
# Test 2: Write with remove_cells for multi-gate design
#---------------------------------------------------------------
puts "--- Test 2: remove_cells on multi-gate design ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog ../../dcalc/test/dcalc_multidriver_test.v
link_design dcalc_multidriver_test

set out_md_basic [make_result_file verilog_remove_md_basic.v]
write_verilog $out_md_basic

# Remove INV_X1
set out_md_inv [make_result_file verilog_remove_md_inv.v]
write_verilog -remove_cells {INV_X1} $out_md_inv
assert_file_nonempty $out_md_inv
assert_file_not_has_cell $out_md_inv INV_X1
assert_file_contains $out_md_inv "module dcalc_multidriver_test"

# Remove AND2_X1
set out_md_and [make_result_file verilog_remove_md_and.v]
write_verilog -remove_cells {AND2_X1} $out_md_and
assert_file_nonempty $out_md_and
assert_file_not_has_cell $out_md_and AND2_X1
assert_file_contains $out_md_and "module dcalc_multidriver_test"

# Remove NAND2_X1 and NOR2_X1
set out_md_gates [make_result_file verilog_remove_md_gates.v]
write_verilog -remove_cells {NAND2_X1 NOR2_X1} $out_md_gates
assert_file_nonempty $out_md_gates
assert_file_not_has_cell $out_md_gates NAND2_X1
assert_file_not_has_cell $out_md_gates NOR2_X1
assert_file_contains $out_md_gates "module dcalc_multidriver_test"

diff_files verilog_remove_md_basic.vok $out_md_basic
diff_files verilog_remove_md_inv.vok $out_md_inv
diff_files verilog_remove_md_and.vok $out_md_and
diff_files verilog_remove_md_gates.vok $out_md_gates

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog $out_md_inv
link_design dcalc_multidriver_test
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {in1 in2 in3 in4 sel}]
set_output_delay -clock clk 0 [get_ports {out1 out2 out3}]
set_input_transition 0.1 [all_inputs]
with_output_to_variable md_rep { report_checks -path_delay max }
if {![regexp {Path Type:\s+max} $md_rep]} {
  error "remove_cells multigate roundtrip timing report missing max path"
}
