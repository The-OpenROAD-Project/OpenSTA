# Test 4: Write hierarchical design
# Exercises: findHierChildren, writeModule for sub-modules, sorted child output
source ../../test/helpers.tcl

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

set out6 [make_result_file verilog_escaped_hier_pwr.v]
write_verilog -include_pwr_gnd $out6

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
