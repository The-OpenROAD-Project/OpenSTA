# Test 5: Write supply/tristate design (special port directions)
# Exercises: verilogPortDir for tristate/supply, writePortDcls
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
# Test 5: Write supply/tristate design (special port directions)
# Exercises: verilogPortDir for tristate/supply, writePortDcls
#   tristate handling, writeAssigns for output aliases
#---------------------------------------------------------------
puts "--- Test 5: write supply/tristate design ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_supply_tristate.v
link_design verilog_supply_tristate

set out7 [make_result_file verilog_escaped_supply.v]
write_verilog $out7
assert_file_nonempty $out7
assert_file_contains $out7 "module verilog_supply_tristate"
assert_file_contains $out7 "tri out1;"
assert_file_contains $out7 "assign out3 = n6;"

set out8 [make_result_file verilog_escaped_supply_pwr.v]
write_verilog -include_pwr_gnd $out8
assert_file_nonempty $out8
assert_file_contains $out8 "module verilog_supply_tristate"
assert_file_contains $out8 "wire gnd_net;"
assert_file_contains $out8 "wire vdd_net;"
