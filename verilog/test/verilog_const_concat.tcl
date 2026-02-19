# Test verilog with net constants (1'b0, 1'b1), concatenation and part selects.
# Targets VerilogReader.cc uncovered paths:
#   makeNetConstant (line 478)
#   VerilogNetConstant constructor/parsing
#   constant net binding (ensureNetBinding with zero_/one_ net names)
#   addConstantNet paths (lines 2190-2192)
#   Multiple design link_design calls (exercises linkNetwork paths)
# Also targets VerilogWriter.cc:
#   writeAssigns, writeChildren with constant connections

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 1: Read verilog with constants
#---------------------------------------------------------------
puts "--- Test 1: read verilog with constants ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_const_concat.v
link_design verilog_const_concat

set cells [get_cells *]
puts "cells: [llength $cells]"

set nets [get_nets *]
puts "nets: [llength $nets]"

set ports [get_ports *]
puts "ports: [llength $ports]"

# Verify cells
foreach cell_name {and_const or_const buf1 inv1 reg1 reg2 reg3 reg4} {
  set inst [get_cells $cell_name]
  set ref [get_property $inst ref_name]
  puts "$cell_name: ref=$ref"
}

#---------------------------------------------------------------
# Test 2: Timing with constant nets
#---------------------------------------------------------------
puts "--- Test 2: timing with constants ---"
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {in1 in2 in3}]
set_output_delay -clock clk 0 [get_ports {out1 out2 out3 out4}]
set_input_transition 10 {in1 in2 in3 clk}

report_checks

report_checks -path_delay min

report_checks -from [get_ports in1] -to [get_ports out1]

report_checks -from [get_ports in2] -to [get_ports out2]

report_checks -fields {slew cap input_pins nets fanout}

#---------------------------------------------------------------
# Test 3: Write verilog
#---------------------------------------------------------------
puts "--- Test 3: write_verilog ---"
set out1 [make_result_file verilog_const_concat_out.v]
write_verilog $out1

set out2 [make_result_file verilog_const_concat_pwr.v]
write_verilog -include_pwr_gnd $out2

if { [file exists $out1] && [file size $out1] > 0 } {
}
if { [file exists $out2] && [file size $out2] > 0 } {
}

#---------------------------------------------------------------
# Test 4: report_net for constant-related nets
#---------------------------------------------------------------
puts "--- Test 4: net reports ---"
foreach net_name {n1 n2 n3 n4} {
  report_net $net_name
  puts "report_net $net_name: done"
}

#---------------------------------------------------------------
# Test 5: Report instances
#---------------------------------------------------------------
puts "--- Test 5: instance reports ---"
foreach inst_name {and_const or_const buf1 inv1 reg1 reg2 reg3 reg4} {
  report_instance $inst_name
}

#---------------------------------------------------------------
# Test 6: Re-read same file (exercises module re-definition path)
#---------------------------------------------------------------
puts "--- Test 6: re-read same verilog ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_const_concat.v
read_verilog verilog_const_concat.v
link_design verilog_const_concat

puts "re-read cells: [llength [get_cells *]]"
puts "re-read nets: [llength [get_nets *]]"

#---------------------------------------------------------------
# Test 7: Read back written verilog (roundtrip)
#---------------------------------------------------------------
puts "--- Test 7: roundtrip ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog $out1
link_design verilog_const_concat

puts "roundtrip cells: [llength [get_cells *]]"
puts "roundtrip nets: [llength [get_nets *]]"
