# Test 6: Write assign/tristate design with removes
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

proc assert_file_not_contains {path token} {
  set in [open $path r]
  set text [read $in]
  close $in
  if {[string first $token $text] >= 0} {
    error "did not expect '$token' in $path"
  }
}

#---------------------------------------------------------------
# Test 6: Write assign/tristate design with removes
#---------------------------------------------------------------
puts "--- Test 6: supply/tristate with removes ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_supply_tristate.v
link_design verilog_supply_tristate

set out_st_rm [make_result_file verilog_remove_supply_buf.v]
write_verilog -remove_cells {BUF_X1} $out_st_rm
assert_file_nonempty $out_st_rm
assert_file_contains $out_st_rm "module verilog_supply_tristate"
assert_file_not_contains $out_st_rm "BUF_X1"

set out_st_pwr [make_result_file verilog_remove_supply_pwr.v]
write_verilog -include_pwr_gnd -remove_cells {INV_X1} $out_st_pwr
assert_file_nonempty $out_st_pwr
assert_file_contains $out_st_pwr "module verilog_supply_tristate"
assert_file_not_contains $out_st_pwr "INV_X1"
assert_file_contains $out_st_pwr "wire gnd_net;"

# Sizes
set sz_st_rm [file size $out_st_rm]
set sz_st_pwr [file size $out_st_pwr]
puts "supply remove sizes: buf=$sz_st_rm inv_pwr=$sz_st_pwr"

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog $out_st_rm
link_design verilog_supply_tristate
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {in1 in2 in3 en}]
set_output_delay -clock clk 0 [get_ports {out1 out2 out3 outbus[*]}]
set_input_transition 0.1 [all_inputs]
with_output_to_variable st_rep { report_checks -path_delay max }
if {![regexp {Path Type:\s+max} $st_rep]} {
  error "remove_cells supply roundtrip timing report missing max path"
}
