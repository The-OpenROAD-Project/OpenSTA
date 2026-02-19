# Comprehensive VerilogReader coverage test
# Exercises: assign, bus ports, concatenation, hierarchical modules,
# ordered port connections, write verilog roundtrip

source ../../test/helpers.tcl

puts "--- Test 1: Read comprehensive verilog ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_coverage_test.v
link_design verilog_coverage_test

set cells [get_cells *]
puts "cells: [llength $cells]"

set nets [get_nets *]
puts "nets: [llength $nets]"

set ports [get_ports *]
puts "ports: [llength $ports]"

puts "--- Test 2: Timing ---"
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [all_inputs]
set_output_delay -clock clk 0 [all_outputs]
set_input_transition 0.1 [all_inputs]

report_checks

puts "--- Test 3: Write verilog ---"
set outfile [make_result_file verilog_coverage_out.v]
write_verilog $outfile
puts "output size: [file size $outfile]"

puts "--- Test 4: Hierarchical queries ---"
set hier [get_cells -hierarchical *]
puts "hierarchical cells: [llength $hier]"
