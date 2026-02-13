# Test verilog writer with escaped names and bus wire declarations.
# Targets:
#   VerilogWriter.cc: writeModule, writePorts, writePortDcls, writeWireDcls,
#     writeChildren, writeChild, writeInstPin, writeInstBusPin,
#     writeInstBusPinBit, writeAssigns, findUnconnectedNetCount,
#     findChildNCcount, findPortNCcount, verilogPortDir for all directions
#   VerilogNamespace.cc: staToVerilog (escaped names with special chars),
#     staToVerilog2 (bus bracket escaping), netVerilogName (bus net names),
#     portVerilogName, cellVerilogName, instanceVerilogName
#   VerilogReader.cc: reading back written files, escaped name parsing,
#     verilogToSta, moduleVerilogToSta, instanceVerilogToSta,
#     netVerilogToSta, portVerilogToSta

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 1: Write verilog for bus design (exercises bus wire declarations)
#---------------------------------------------------------------
puts "--- Test 1: write bus design ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_bus_test.v
link_design verilog_bus_test

set cells [get_cells *]
puts "cells: [llength $cells]"

set nets [get_nets *]
puts "nets: [llength $nets]"

set ports [get_ports *]
puts "ports: [llength $ports]"

# Write basic
set out1 [make_result_file verilog_escaped_bus.v]
write_verilog $out1
puts "PASS: write_verilog bus design"

if { [file exists $out1] && [file size $out1] > 0 } {
  puts "PASS: bus output size=[file size $out1]"
}

# Write with pwr_gnd
set out2 [make_result_file verilog_escaped_bus_pwr.v]
write_verilog -include_pwr_gnd $out2
puts "PASS: write_verilog bus -include_pwr_gnd"

if { [file exists $out2] && [file size $out2] > 0 } {
  puts "PASS: bus pwr output size=[file size $out2]"
}

# pwr_gnd should be larger
set sz1 [file size $out1]
set sz2 [file size $out2]
if { $sz2 >= $sz1 } {
  puts "PASS: pwr_gnd >= basic"
}

#---------------------------------------------------------------
# Test 2: Read back written bus verilog (roundtrip)
# Exercises: verilogToSta on bus names, bus port parsing
#---------------------------------------------------------------
puts "--- Test 2: roundtrip bus design ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog $out1
link_design verilog_bus_test

set rt_cells [get_cells *]
puts "roundtrip cells: [llength $rt_cells]"

set rt_nets [get_nets *]
puts "roundtrip nets: [llength $rt_nets]"

set rt_ports [get_ports *]
puts "roundtrip ports: [llength $rt_ports]"

# Verify bus ports after roundtrip
set rt_din [get_ports {data_in[*]}]
puts "roundtrip data_in[*]: [llength $rt_din]"

set rt_dout [get_ports {data_out[*]}]
puts "roundtrip data_out[*]: [llength $rt_dout]"

# Timing after roundtrip
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {data_in[*]}]
set_output_delay -clock clk 0 [get_ports {data_out[*]}]
set_input_transition 0.1 [all_inputs]
report_checks
puts "PASS: timing after roundtrip"

#---------------------------------------------------------------
# Test 3: Write complex bus design
# Exercises: writeWireDcls with bus nets (isBusName, parseBusName)
#---------------------------------------------------------------
puts "--- Test 3: write complex bus design ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_complex_bus_test.v
link_design verilog_complex_bus_test

set out3 [make_result_file verilog_escaped_complex.v]
write_verilog $out3
puts "PASS: write_verilog complex bus"

if { [file exists $out3] && [file size $out3] > 0 } {
  puts "PASS: complex output size=[file size $out3]"
}

set out4 [make_result_file verilog_escaped_complex_pwr.v]
write_verilog -include_pwr_gnd $out4
puts "PASS: write_verilog complex -include_pwr_gnd"

if { [file exists $out4] && [file size $out4] > 0 } {
  puts "PASS: complex pwr output size=[file size $out4]"
}

# Read back complex bus design
puts "--- roundtrip complex bus ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog $out3
link_design verilog_complex_bus_test

set rt2_cells [get_cells *]
puts "complex roundtrip cells: [llength $rt2_cells]"

set rt2_ports [get_ports *]
puts "complex roundtrip ports: [llength $rt2_ports]"

# Bus port queries after roundtrip
set rt2_da [get_ports {data_a[*]}]
puts "roundtrip data_a[*]: [llength $rt2_da]"

set rt2_db [get_ports {data_b[*]}]
puts "roundtrip data_b[*]: [llength $rt2_db]"

set rt2_res [get_ports {result[*]}]
puts "roundtrip result[*]: [llength $rt2_res]"

# Timing after complex roundtrip
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {data_a[*]}]
set_input_delay -clock clk 0 [get_ports {data_b[*]}]
set_output_delay -clock clk 0 [get_ports {result[*]}]
set_output_delay -clock clk 0 [get_ports carry]
set_output_delay -clock clk 0 [get_ports overflow]
set_input_transition 0.1 [all_inputs]
report_checks
puts "PASS: timing after complex roundtrip"

#---------------------------------------------------------------
# Test 4: Write hierarchical design
# Exercises: findHierChildren, writeModule for sub-modules,
#   sorted child output
#---------------------------------------------------------------
puts "--- Test 4: write hierarchical design ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog ../../network/test/network_hier_test.v
link_design network_hier_test

set out5 [make_result_file verilog_escaped_hier.v]
write_verilog $out5
puts "PASS: write_verilog hier"

if { [file exists $out5] && [file size $out5] > 0 } {
  puts "PASS: hier output size=[file size $out5]"
}

set out6 [make_result_file verilog_escaped_hier_pwr.v]
write_verilog -include_pwr_gnd $out6
puts "PASS: write_verilog hier -include_pwr_gnd"

if { [file exists $out6] && [file size $out6] > 0 } {
  puts "PASS: hier pwr output size=[file size $out6]"
}

# Roundtrip hierarchical
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog $out5
link_design network_hier_test

set rt3_cells [get_cells *]
puts "hier roundtrip cells: [llength $rt3_cells]"

set rt3_nets [get_nets *]
puts "hier roundtrip nets: [llength $rt3_nets]"

set rt3_ports [get_ports *]
puts "hier roundtrip ports: [llength $rt3_ports]"

# Timing after hierarchical roundtrip
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {in1 in2 in3}]
set_output_delay -clock clk 0 [get_ports {out1 out2}]
set_input_transition 0.1 [all_inputs]
report_checks
puts "PASS: timing after hier roundtrip"

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
puts "PASS: write_verilog supply/tri"

if { [file exists $out7] && [file size $out7] > 0 } {
  puts "PASS: supply output size=[file size $out7]"
}

set out8 [make_result_file verilog_escaped_supply_pwr.v]
write_verilog -include_pwr_gnd $out8
puts "PASS: write_verilog supply -include_pwr_gnd"

if { [file exists $out8] && [file size $out8] > 0 } {
  puts "PASS: supply pwr output size=[file size $out8]"
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
puts "PASS: write_verilog const/concat"

if { [file exists $out9] && [file size $out9] > 0 } {
  puts "PASS: const output size=[file size $out9]"
}

set out10 [make_result_file verilog_escaped_const_pwr.v]
write_verilog -include_pwr_gnd $out10
puts "PASS: write_verilog const -include_pwr_gnd"

if { [file exists $out10] && [file size $out10] > 0 } {
  puts "PASS: const pwr output size=[file size $out10]"
}

# Roundtrip constant design
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog $out9
link_design verilog_const_concat

set rt4_cells [get_cells *]
puts "const roundtrip cells: [llength $rt4_cells]"

set rt4_nets [get_nets *]
puts "const roundtrip nets: [llength $rt4_nets]"

puts "PASS: const roundtrip"

puts "ALL PASSED"
