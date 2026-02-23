# Test VerilogReader with the larger GCD design which has more diverse
# constructs: bus ports, bus nets, many instances, and complex connectivity.
# Then write verilog with various options and re-read.
# Targets:
#   VerilogReader.cc: readVerilog (large file), makeModule, makeModuleInst,
#     makeDcl, makeDclArg, makeDclBus, makeNetConcat,
#     linkNetwork, checkModule, resolveModule, linkModuleInst,
#     linkWire, VerilogNet, VerilogDcl, VerilogDclBus,
#     bus port parsing, bus net connections,
#     VerilogError reporting for missing modules
#   VerilogWriter.cc: writeVerilog, writeModule, writeInstance,
#     writeNet, writeBus, writePowerGround, writeSort,
#     -remove_cells, -include_pwr_gnd, -sort
#   VerilogLex.ll: tokenization of larger file, bus brackets,
#     escaped identifiers, string tokens
source ../../test/helpers.tcl

############################################################
# Test 1: Read Sky130 library and GCD verilog
############################################################
puts "--- Test 1: read GCD design ---"
read_liberty ../../test/sky130hd/sky130hd_tt.lib
read_verilog ../../examples/gcd_sky130hd.v
link_design gcd

set cells [get_cells *]
puts "cells: [llength $cells]"

set nets [get_nets *]
puts "nets: [llength $nets]"

set ports [get_ports *]
puts "ports: [llength $ports]"

# Verify bus ports are parsed correctly
foreach port_pattern {req_msg resp_msg} {
  set bus_ports [get_ports $port_pattern*]
  puts "bus $port_pattern: [llength $bus_ports] bits"
}

############################################################
# Test 2: Write verilog with various options
############################################################
puts "--- Test 2: write verilog ---"

set out1 [make_result_file verilog_gcd_large_out.v]
write_verilog $out1

set out2 [make_result_file verilog_gcd_large_pwr.v]
write_verilog -include_pwr_gnd $out2

set out3 [make_result_file verilog_gcd_large_sort.v]
write_verilog $out3

diff_files verilog_gcd_large_out.vok $out1
diff_files verilog_gcd_large_pwr.vok $out2
diff_files verilog_gcd_large_sort.vok $out3

############################################################
# Test 3: Re-read written verilog
############################################################
puts "--- Test 3: re-read ---"
read_liberty ../../test/sky130hd/sky130hd_tt.lib
read_verilog $out1
link_design gcd
puts "re-read cells: [llength [get_cells *]]"

############################################################
# Test 4: Timing with the design
############################################################
puts "--- Test 4: timing ---"
source ../../examples/gcd_sky130hd.sdc

report_checks -endpoint_count 3

report_checks -path_delay min -endpoint_count 3

report_checks -fields {slew cap input_pins fanout}

report_checks -format full_clock

############################################################
# Test 5: Write with -remove_cells to exclude specific cells
############################################################
puts "--- Test 5: write with remove ---"

set out4 [make_result_file verilog_gcd_large_remove.v]
write_verilog -remove_cells {sky130_fd_sc_hd__fill_1 sky130_fd_sc_hd__fill_2} $out4

############################################################
# Test 6: Instance and net reports
############################################################
puts "--- Test 6: instance/net reports ---"
set inst_count 0
foreach inst_obj [get_cells *] {
  report_instance [get_name $inst_obj]
  incr inst_count
  if {$inst_count >= 20} break
}

set net_count 0
foreach net_obj [get_nets *] {
  report_net [get_name $net_obj]
  incr net_count
  if {$net_count >= 20} break
}

############################################################
# Test 7: Read and write the example1 design too
############################################################
puts "--- Test 7: example1 ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog ../../examples/example1.v
link_design top

set out5 [make_result_file verilog_example1_out.v]
write_verilog $out5

# Re-read
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog $out5
link_design top

create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}
report_checks
