# Test 1: Write verilog for bus design (exercises bus wire declarations)
# Test 2: Read back written bus verilog (roundtrip)
source ../../test/helpers.tcl
suppress_msg 1140

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

# Write with pwr_gnd
set out2 [make_result_file verilog_escaped_bus_pwr.v]
write_verilog -include_pwr_gnd $out2

diff_files verilog_escaped_bus.vok $out1
diff_files verilog_escaped_bus_pwr.vok $out2

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
