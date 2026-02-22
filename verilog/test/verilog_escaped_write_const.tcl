# Test 6: Write constant/concat design
# Exercises: writeChildren with constant pin connections
source ../../test/helpers.tcl

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

set out10 [make_result_file verilog_escaped_const_pwr.v]
write_verilog -include_pwr_gnd $out10

# Roundtrip constant design
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog $out9
link_design verilog_const_concat

set rt4_cells [get_cells *]
puts "const roundtrip cells: [llength $rt4_cells]"

set rt4_nets [get_nets *]
puts "const roundtrip nets: [llength $rt4_nets]"
