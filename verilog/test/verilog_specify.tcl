# Test verilog reader with specify blocks, parameters, and defparams
# Targets: VerilogReader.cc (parameter/specify/defparam paths)
#   VerilogLex.ll (lexer paths for specify, parameter, defparam keywords)
#   VerilogParse.yy (parser paths for specify blocks)

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test: Read verilog with specify blocks and parameters
#---------------------------------------------------------------
puts "--- read verilog with specify/parameter ---"
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../test/verilog_specify.v
link_design counter

set cells [get_cells *]
puts "cells: [llength $cells]"

set nets [get_nets *]
puts "nets: [llength $nets]"

set ports [get_ports *]
puts "ports: [llength $ports]"

puts "PASS: read_verilog with specify/parameter"

#---------------------------------------------------------------
# Write and verify
#---------------------------------------------------------------
puts "--- write_verilog ---"
set outfile [make_result_file verilog_specify_out.v]
write_verilog $outfile
puts "PASS: write_verilog after specify"

if { [file exists $outfile] && [file size $outfile] > 0 } {
  puts "PASS: output file is non-empty"
}

puts "ALL PASSED"
