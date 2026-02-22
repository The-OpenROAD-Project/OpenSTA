# Test advanced verilog writer options - ASAP7 design
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
# Test 1: Write verilog from ASAP7 design (has more complexity)
#---------------------------------------------------------------
puts "--- Test 1: ASAP7 write ---"
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz

read_verilog ../../test/reg1_asap7.v
link_design top

puts "cells: [llength [get_cells *]]"
puts "nets: [llength [get_nets *]]"
puts "ports: [llength [get_ports *]]"

# Write basic
set out1 [make_result_file verilog_advanced_out1.v]
write_verilog $out1
assert_file_nonempty $out1
assert_file_contains $out1 "module top"
assert_file_contains $out1 "BUFx2_ASAP7_75t_R"

# Write with pwr_gnd
set out2 [make_result_file verilog_advanced_out2.v]
write_verilog -include_pwr_gnd $out2
assert_file_nonempty $out2
assert_file_contains $out2 "module top"

# Write with remove_cells
set out3 [make_result_file verilog_advanced_out3.v]
write_verilog -remove_cells {} $out3
assert_file_nonempty $out3
assert_file_contains $out3 "module top"

# Compare sizes
set sz1 [file size $out1]
set sz2 [file size $out2]
set sz3 [file size $out3]
puts "basic size: $sz1"
puts "pwr_gnd size: $sz2"
puts "remove_cells size: $sz3"

set in1 [open $out1 r]
set txt1 [read $in1]
close $in1
set in3 [open $out3 r]
set txt3 [read $in3]
close $in3
if {$txt1 ne $txt3} {
  error "empty -remove_cells output should match baseline output"
}

create_clock -name clk -period 10 [get_ports {clk1 clk2 clk3}]
set_input_delay -clock clk 0 [get_ports {in1 in2}]
set_output_delay -clock clk 0 [get_ports out]
set_input_transition 0.1 [all_inputs]
with_output_to_variable asap7_rep { report_checks -path_delay max }
if {![regexp {Path Type:\s+max} $asap7_rep]} {
  error "writer_asap7 timing report missing max path"
}
