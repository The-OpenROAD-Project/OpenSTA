# Test advanced verilog writer options - Write after modification
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
# Test 2: Write after network modification
#---------------------------------------------------------------
puts "--- Test 2: Write after modification ---"

# Need to load a design first to modify
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_test1.v
link_design verilog_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_output_delay -clock clk 0 [get_ports out1]
set_input_transition 0.1 [all_inputs]
with_output_to_variable base_rep { report_checks -path_delay max }
if {![regexp {Path Type:\s+max} $base_rep]} {
  error "baseline timing report missing max path"
}

# Add an instance and net
set new_net [make_net extra_net]
set new_inst [make_instance extra_buf NangateOpenCellLibrary/BUF_X2]
connect_pin extra_net extra_buf/A
with_output_to_variable mod_rep { report_checks -path_delay max }
if {![regexp {Path Type:\s+max} $mod_rep]} {
  error "modified timing report missing max path"
}

set out4 [make_result_file verilog_advanced_out4.v]
write_verilog $out4
assert_file_nonempty $out4
assert_file_contains $out4 "module verilog_test1"
assert_file_contains $out4 "extra_buf"

set sz4 [file size $out4]
puts "modified size: $sz4"
# Disconnect and delete
disconnect_pin extra_net extra_buf/A
delete_instance extra_buf
delete_net extra_net
with_output_to_variable final_rep { report_checks -path_delay max }
if {![regexp {Path Type:\s+max} $final_rep]} {
  error "post-cleanup timing report missing max path"
}
