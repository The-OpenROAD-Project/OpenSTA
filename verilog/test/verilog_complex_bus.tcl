# Test verilog with complex bus/range constructs

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 1: Read complex bus verilog
#---------------------------------------------------------------
puts "--- Test 1: read complex bus verilog ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_complex_bus_test.v
link_design verilog_complex_bus_test

set cells [get_cells *]
puts "cells: [llength $cells]"

set nets [get_nets *]
puts "nets: [llength $nets]"

set ports [get_ports *]
puts "ports: [llength $ports]"

#---------------------------------------------------------------
# Test 2: Query 8-bit bus ports
#---------------------------------------------------------------
puts "--- Test 2: bus port queries ---"

# Query bus ports
set data_a_ports [get_ports data_a*]
puts "data_a* ports: [llength $data_a_ports]"

set data_b_ports [get_ports data_b*]
puts "data_b* ports: [llength $data_b_ports]"

set result_ports [get_ports result*]
puts "result* ports: [llength $result_ports]"

# Query individual bits
foreach i {0 1 2 3 4 5 6 7} {
  set p [get_ports "data_a\[$i\]"]
  puts "data_a\[$i\]: [get_property $p direction]"
}

foreach i {0 1 2 3 4 5 6 7} {
  set p [get_ports "result\[$i\]"]
  puts "result\[$i\]: [get_property $p direction]"
}

# Scalar ports
set carry_port [get_ports carry]
puts "carry direction: [get_property $carry_port direction]"

set overflow_port [get_ports overflow]
puts "overflow direction: [get_property $overflow_port direction]"

#---------------------------------------------------------------
# Test 3: Query bus wires and nets
#---------------------------------------------------------------
puts "--- Test 3: bus wire queries ---"

set stage1_nets [get_nets stage1*]
puts "stage1* nets: [llength $stage1_nets]"

set stage2_nets [get_nets stage2*]
puts "stage2* nets: [llength $stage2_nets]"

# Query individual wire bits
foreach i {0 1 7} {
  set n [get_nets "stage1\[$i\]"]
  puts "stage1\[$i\]: [get_full_name $n]"
  set n [get_nets "stage2\[$i\]"]
  puts "stage2\[$i\]: [get_full_name $n]"
}

# Wildcard bus queries
set wild_stage1 [get_nets {stage1[*]}]
puts "stage1\[*\] nets: [llength $wild_stage1]"

set wild_stage2 [get_nets {stage2[*]}]
puts "stage2\[*\] nets: [llength $wild_stage2]"

#---------------------------------------------------------------
# Test 4: Query pins on cells connected to buses
#---------------------------------------------------------------
puts "--- Test 4: bus pin queries ---"

# Pins on buffer cells
set buf_a0_pins [get_pins buf_a0/*]
puts "buf_a0 pins: [llength $buf_a0_pins]"
foreach p $buf_a0_pins {
  puts "  [get_full_name $p] dir=[get_property $p direction]"
}

# Pins on AND cells
set and0_pins [get_pins and0/*]
puts "and0 pins: [llength $and0_pins]"
foreach p $and0_pins {
  puts "  [get_full_name $p] dir=[get_property $p direction]"
}

# Pins on register cells
set reg0_pins [get_pins reg0/*]
puts "reg0 pins: [llength $reg0_pins]"
foreach p $reg0_pins {
  puts "  [get_full_name $p] dir=[get_property $p direction]"
}

# Wildcard pin queries
set all_A_pins [get_pins */A]
puts "*/A pins: [llength $all_A_pins]"

set all_Z_pins [get_pins */Z]
puts "*/Z pins: [llength $all_Z_pins]"

set all_ZN_pins [get_pins */ZN]
puts "*/ZN pins: [llength $all_ZN_pins]"

set all_D_pins [get_pins */D]
puts "*/D pins: [llength $all_D_pins]"

set all_Q_pins [get_pins */Q]
puts "*/Q pins: [llength $all_Q_pins]"

set all_CK_pins [get_pins */CK]
puts "*/CK pins: [llength $all_CK_pins]"

#---------------------------------------------------------------
# Test 5: Write verilog with bus ports
# Exercises writeInstBusPin, writeInstBusPinBit paths
#---------------------------------------------------------------
puts "--- Test 5: write verilog with buses ---"

set outfile [make_result_file verilog_complex_bus_out.v]
write_verilog $outfile

diff_files verilog_complex_bus_out.vok $outfile

set outfile2 [make_result_file verilog_complex_bus_pwr.v]
write_verilog -include_pwr_gnd $outfile2

#---------------------------------------------------------------
# Test 6: Timing analysis on bus design
#---------------------------------------------------------------
puts "--- Test 6: timing analysis ---"
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {data_a[*]}]
set_input_delay -clock clk 0 [get_ports {data_b[*]}]
set_output_delay -clock clk 0 [get_ports {result[*]}]
set_output_delay -clock clk 0 [get_ports carry]
set_output_delay -clock clk 0 [get_ports overflow]
set_input_transition 10 [all_inputs]

report_checks

report_checks -path_delay min

# Specific paths through bus
report_checks -from [get_ports {data_a[0]}] -to [get_ports {result[0]}]

report_checks -from [get_ports {data_a[7]}] -to [get_ports {result[7]}]

report_checks -to [get_ports carry]

report_checks -to [get_ports overflow]

report_checks -fields {slew cap input_pins fanout}

#---------------------------------------------------------------
# Test 7: Report nets on bus nets
#---------------------------------------------------------------
puts "--- Test 7: report_net on bus ---"
foreach net {stage1[0] stage1[7] stage2[0] stage2[7] internal_carry internal_overflow} {
  report_net $net
  puts "report_net $net: done"
}

#---------------------------------------------------------------
# Test 8: Fanin/fanout through bus
#---------------------------------------------------------------
puts "--- Test 8: fanin/fanout ---"
set fi [get_fanin -to [get_ports {result[0]}] -flat]
puts "fanin to result[0]: [llength $fi]"

set fo [get_fanout -from [get_ports {data_a[0]}] -flat]
puts "fanout from data_a[0]: [llength $fo]"

set fi_cells [get_fanin -to [get_ports carry] -only_cells]
puts "fanin cells to carry: [llength $fi_cells]"
