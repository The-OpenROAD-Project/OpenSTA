# Test advanced verilog writer options - Nangate45 write
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

#---------------------------------------------------------------
# Test 4: Write verilog for Nangate45 design
#---------------------------------------------------------------
puts "--- Test 4: Nangate45 write ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_test1.v
link_design verilog_test1

set out7 [make_result_file verilog_advanced_out7.v]
write_verilog $out7
assert_file_nonempty $out7
assert_file_contains $out7 "module verilog_test1"
assert_file_contains $out7 "BUF_X1"

set out8 [make_result_file verilog_advanced_out8.v]
write_verilog -include_pwr_gnd $out8
assert_file_nonempty $out8
assert_file_contains $out8 "module verilog_test1"

set sz7 [file size $out7]
set sz8 [file size $out8]
puts "nangate45 basic: $sz7, pwr_gnd: $sz8"

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_output_delay -clock clk 0 [get_ports out1]
set_input_transition 0.1 [all_inputs]
with_output_to_variable ng_rep { report_checks -path_delay max }
if {![regexp {Path Type:\s+max} $ng_rep]} {
  error "writer_nangate timing report missing max path"
}
