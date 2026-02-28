# Test advanced verilog writer options - Sky130 with attributes
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

#---------------------------------------------------------------
# Test 3: Write verilog for sky130 design with attributes
#---------------------------------------------------------------
puts "--- Test 3: Sky130 with attributes ---"
# Reset
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../test/verilog_attribute.v
link_design counter

set out5 [make_result_file verilog_advanced_out5.v]
write_verilog $out5
assert_file_nonempty $out5
diff_files verilog_advanced_out5.vok $out5
assert_file_contains $out5 "module counter"
assert_file_contains $out5 "sky130_fd_sc_hd__dfrtp_1"

set out6 [make_result_file verilog_advanced_out6.v]
write_verilog -include_pwr_gnd $out6
assert_file_nonempty $out6
diff_files verilog_advanced_out6.vok $out6
assert_file_contains $out6 "module counter"

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {reset in}]
set_output_delay -clock clk 0 [get_ports out]
set_input_transition 0.1 [all_inputs]
with_output_to_variable sky_rep { report_checks -path_delay max }
if {![regexp {Path Type:\s+max} $sky_rep]} {
  error "writer_sky130 timing report missing max path"
}
