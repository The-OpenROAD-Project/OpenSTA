# Test reading more complex verilog with sky130 library
# Using verilog_attribute.v which has Yosys-style attributes and sky130 cells
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../test/verilog_attribute.v
link_design counter

puts "--- query cells ---"
set cells [get_cells *]
puts "cells count: [llength $cells]"

puts "--- query nets ---"
set nets [get_nets *]
puts "nets count: [llength $nets]"

puts "--- query pins ---"
set all_pins [get_pins */*]
puts "pins count: [llength $all_pins]"

puts "--- query ports ---"
set ports [get_ports *]
puts "ports count: [llength $ports]"

puts "--- create_clock ---"
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in]
set_input_delay -clock clk 0 [get_ports reset]
set_output_delay -clock clk 0 [get_ports out]
puts "PASS: clock and constraints created"

puts "--- report_checks ---"
report_checks
puts "PASS: report_checks completed"

puts "--- report_checks -path_delay min ---"
report_checks -path_delay min
puts "PASS: report_checks min completed"

puts "--- get_cells with filter ---"
set dff_cells [get_cells -filter "ref_name == sky130_fd_sc_hd__dfrtp_1" *]
puts "dfrtp cells count: [llength $dff_cells]"

puts "--- report_instance for first cell ---"
report_instance _1415_

puts "--- report_net mid ---"
report_net mid

puts "--- all_inputs ---"
set inputs [all_inputs]
puts "all_inputs count: [llength $inputs]"

puts "--- all_outputs ---"
set outputs [all_outputs]
puts "all_outputs count: [llength $outputs]"

puts "--- all_clocks ---"
set clocks [all_clocks]
puts "all_clocks count: [llength $clocks]"

puts "ALL PASSED"
