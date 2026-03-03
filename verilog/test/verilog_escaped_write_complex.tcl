# Test 3: Write complex bus design
# Exercises: writeWireDcls with bus nets (isBusName, parseBusName)
source ../../test/helpers.tcl
suppress_msg 1140

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

set out4 [make_result_file verilog_escaped_complex_pwr.v]
write_verilog -include_pwr_gnd $out4

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
