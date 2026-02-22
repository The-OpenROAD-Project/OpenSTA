# Test 5: Write supply/tristate design (special port directions)
# Exercises: verilogPortDir for tristate/supply, writePortDcls
source ../../test/helpers.tcl

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

set out8 [make_result_file verilog_escaped_supply_pwr.v]
write_verilog -include_pwr_gnd $out8
