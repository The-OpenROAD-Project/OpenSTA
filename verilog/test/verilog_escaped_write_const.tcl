# Test 6: Write constant/concat design
# Exercises: writeChildren with constant pin connections
source ../../test/helpers.tcl
suppress_msg 1140

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

#---------------------------------------------------------------
# Test 6: Write constant/concat design
# Exercises: writeChildren with constant pin connections
#---------------------------------------------------------------
puts "--- Test 6: write constant design ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_const_concat.v
link_design verilog_const_concat

set out9 [make_result_file verilog_escaped_const.v]
write_verilog $out9
assert_file_nonempty $out9
assert_file_contains $out9 "module verilog_const_concat"
assert_file_contains $out9 "one_"
assert_file_contains $out9 "zero_"

set out10 [make_result_file verilog_escaped_const_pwr.v]
write_verilog -include_pwr_gnd $out10
assert_file_nonempty $out10
assert_file_contains $out10 "module verilog_const_concat"

# Roundtrip constant design
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog $out9
link_design verilog_const_concat

set rt4_cells [get_cells *]
puts "const roundtrip cells: [llength $rt4_cells]"
if {[llength $rt4_cells] < 8} {
  error "unexpected roundtrip cell count: [llength $rt4_cells]"
}

set rt4_nets [get_nets *]
puts "const roundtrip nets: [llength $rt4_nets]"
if {[llength $rt4_nets] < 10} {
  error "unexpected roundtrip net count: [llength $rt4_nets]"
}

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {in1 in2 in3}]
set_output_delay -clock clk 0 [get_ports {out1 out2 out3 out4}]
set_input_transition 0.1 [all_inputs]

with_output_to_variable rt4_rep { report_checks }
if {![regexp {Path Type:\s+max} $rt4_rep]} {
  error "roundtrip timing report missing max path section"
}
