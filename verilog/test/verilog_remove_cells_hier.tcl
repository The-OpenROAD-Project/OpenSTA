# Test 7: Write hierarchical design with removes
# Exercises: findHierChildren, writeChild remove path
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
# Test 7: Write hierarchical design with removes
# Exercises: findHierChildren, writeChild remove path
#---------------------------------------------------------------
puts "--- Test 7: hierarchical with removes ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog ../../network/test/network_hier_test.v
link_design network_hier_test

set out_h_rm [make_result_file verilog_remove_hier_buf.v]
write_verilog -remove_cells {BUF_X1} $out_h_rm
assert_file_nonempty $out_h_rm
assert_file_contains $out_h_rm "module network_hier_test"
assert_file_not_contains $out_h_rm "BUF_X1"

set out_h_rm2 [make_result_file verilog_remove_hier_and.v]
write_verilog -remove_cells {AND2_X1 INV_X1} $out_h_rm2
assert_file_nonempty $out_h_rm2
assert_file_contains $out_h_rm2 "module network_hier_test"
assert_file_not_contains $out_h_rm2 "AND2_X1"
assert_file_not_contains $out_h_rm2 "INV_X1"

diff_files verilog_remove_hier_buf.vok $out_h_rm
diff_files verilog_remove_hier_and.vok $out_h_rm2

# Read back hierarchical with removes
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog $out_h_rm
link_design network_hier_test

set rt_h_cells [get_cells *]
puts "hier roundtrip cells: [llength $rt_h_cells]"
if {[llength $rt_h_cells] == 0} {
  error "roundtrip hierarchy has no cells"
}

set rt_h_hier [get_cells -hierarchical *]
puts "hier roundtrip hier cells: [llength $rt_h_hier]"
if {[llength $rt_h_hier] < [llength $rt_h_cells]} {
  error "hierarchical cell count is smaller than flat cell count"
}

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {in1 in2 in3}]
set_output_delay -clock clk 0 [get_ports {out1 out2}]
set_input_transition 0.1 [all_inputs]
with_output_to_variable hier_rep { report_checks -path_delay max }
if {![regexp {Path Type:\s+max} $hier_rep]} {
  error "hierarchical remove_cells roundtrip timing report missing max path"
}
