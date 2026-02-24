# Test verilog writer with larger GCD design (sky130hd)
# Targets: VerilogWriter.cc (writeModules, findHierChildren, writeChildren,
#   writeChild, writeInstPin, writeWireDcls, writePorts, writePortDcls,
#   verilogPortDir for various directions, findUnconnectedNetCount)
# Also targets: VerilogReader.cc (bus port reading, large netlist parsing,
#   makeModuleInst, linkNetwork, various cell types, declaration handling)

source ../../test/helpers.tcl
suppress_msg 1140

#---------------------------------------------------------------
# Test 1: Read and write GCD sky130hd design (large design)
#---------------------------------------------------------------
puts "--- Test 1: read GCD sky130hd ---"
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../examples/gcd_sky130hd.v
link_design gcd

set cells [get_cells *]
puts "cells: [llength $cells]"

set nets [get_nets *]
puts "nets: [llength $nets]"

set ports [get_ports *]
puts "ports: [llength $ports]"

# Check bus ports exist
set req_msg_ports [get_ports req_msg*]
puts "req_msg* ports: [llength $req_msg_ports]"

set resp_msg_ports [get_ports resp_msg*]
puts "resp_msg* ports: [llength $resp_msg_ports]"

# Query specific ports
foreach pname {clk reset req_val req_rdy resp_val resp_rdy} {
  set p [get_ports $pname]
  puts "$pname dir=[get_property $p direction]"
}

#---------------------------------------------------------------
# Test 2: Write verilog - basic
#---------------------------------------------------------------
puts "--- Test 2: write_verilog basic ---"
set out1 [make_result_file verilog_hier_basic.v]
write_verilog $out1

#---------------------------------------------------------------
# Test 3: Write verilog with -include_pwr_gnd
#---------------------------------------------------------------
puts "--- Test 3: write_verilog -include_pwr_gnd ---"
set out2 [make_result_file verilog_hier_pwr.v]
write_verilog -include_pwr_gnd $out2

diff_files verilog_hier_basic.vok $out1
diff_files verilog_hier_pwr.vok $out2

#---------------------------------------------------------------
# Test 4: Write verilog with -remove_cells
#---------------------------------------------------------------
puts "--- Test 4: write_verilog -remove_cells ---"
set out3 [make_result_file verilog_hier_remove.v]
write_verilog -remove_cells {} $out3

#---------------------------------------------------------------
# Test 5: Read back the written verilog (roundtrip)
#---------------------------------------------------------------
puts "--- Test 5: read back written verilog ---"
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog $out1
link_design gcd

set cells2 [get_cells *]
puts "roundtrip cells: [llength $cells2]"

set nets2 [get_nets *]
puts "roundtrip nets: [llength $nets2]"

set ports2 [get_ports *]
puts "roundtrip ports: [llength $ports2]"

# Write again after roundtrip
set out4 [make_result_file verilog_hier_roundtrip.v]
write_verilog $out4

#---------------------------------------------------------------
# Test 6: Set up timing and report with bus ports
#---------------------------------------------------------------
puts "--- Test 6: timing with bus ports ---"
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {reset req_val req_msg* resp_rdy}]
set_output_delay -clock clk 0 [all_outputs]

report_checks

report_checks -path_delay min

report_checks -fields {slew cap input_pins fanout}

#---------------------------------------------------------------
# Test 7: Write verilog after timing setup (tests more writer paths)
#---------------------------------------------------------------
puts "--- Test 7: write after timing setup ---"
set out5 [make_result_file verilog_hier_post_timing.v]
write_verilog $out5

set out6 [make_result_file verilog_hier_post_timing_pwr.v]
write_verilog -include_pwr_gnd $out6
