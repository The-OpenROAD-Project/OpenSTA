# Test verilog reader with attributes and complex constructs
# Targets: VerilogReader.cc (attribute handling paths - 52.7% coverage)
#   VerilogReaderPvt.hh (61.0% coverage)
#   VerilogLex.ll (54.7% coverage - lexer paths)
#   VerilogParse.yy (41.9% coverage - parser paths)

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 1: Read verilog with Yosys-style attributes
#---------------------------------------------------------------
puts "--- Test 1: Yosys attributes ---"
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../test/verilog_attribute.v
link_design counter

set cells [get_cells *]
puts "cells: [llength $cells]"

set nets [get_nets *]
puts "nets: [llength $nets]"

set ports [get_ports *]
puts "ports: [llength $ports]"

set pins [get_pins */*]
puts "pins: [llength $pins]"

# Check that cell attributes were stored
foreach cell_name {_1415_} {
  set inst [get_cells $cell_name]
  set ref [get_property $inst ref_name]
  puts "$cell_name ref: $ref"
}

# Report timing to exercise attribute-bearing instances
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in]
set_input_delay -clock clk 0 [get_ports reset]
set_output_delay -clock clk 0 [get_ports out]

report_checks

report_checks -path_delay min

report_checks -fields {slew cap input_pins}

#---------------------------------------------------------------
# Write verilog and read back
#---------------------------------------------------------------
puts "--- write_verilog and read back ---"
set outfile [make_result_file verilog_attr_out.v]
write_verilog $outfile

if { [file exists $outfile] && [file size $outfile] > 0 } {
} else {
  puts "FAIL: output file issue"
}

# Write with include_pwr_gnd
set outfile2 [make_result_file verilog_attr_pwr.v]
write_verilog -include_pwr_gnd $outfile2

if { [file exists $outfile2] && [file size $outfile2] > 0 } {
}
