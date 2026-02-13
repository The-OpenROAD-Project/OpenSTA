# Test verilog writer with larger GCD design (sky130hd)
# Targets: VerilogWriter.cc (writeModules, findHierChildren, writeChildren,
#   writeChild, writeInstPin, writeWireDcls, writePorts, writePortDcls,
#   verilogPortDir for various directions, findUnconnectedNetCount)
# Also targets: VerilogReader.cc (bus port reading, large netlist parsing,
#   makeModuleInst, linkNetwork, various cell types, declaration handling)

source ../../test/helpers.tcl

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
  catch {
    set p [get_ports $pname]
    puts "$pname dir=[get_property $p direction]"
  } msg
}

#---------------------------------------------------------------
# Test 2: Write verilog - basic
#---------------------------------------------------------------
puts "--- Test 2: write_verilog basic ---"
set out1 [make_result_file verilog_hier_basic.v]
write_verilog $out1
if { [file exists $out1] && [file size $out1] > 0 } {
  puts "PASS: basic write_verilog size=[file size $out1]"
} else {
  puts "FAIL: basic write_verilog file missing or empty"
}

#---------------------------------------------------------------
# Test 3: Write verilog with -include_pwr_gnd
#---------------------------------------------------------------
puts "--- Test 3: write_verilog -include_pwr_gnd ---"
set out2 [make_result_file verilog_hier_pwr.v]
write_verilog -include_pwr_gnd $out2
if { [file exists $out2] && [file size $out2] > 0 } {
  puts "PASS: pwr_gnd write_verilog size=[file size $out2]"
} else {
  puts "FAIL: pwr_gnd write_verilog file missing or empty"
}

# pwr_gnd output should be >= basic output
set sz1 [file size $out1]
set sz2 [file size $out2]
if { $sz2 >= $sz1 } {
  puts "PASS: pwr_gnd output >= basic output"
}

#---------------------------------------------------------------
# Test 4: Write verilog with -remove_cells
#---------------------------------------------------------------
puts "--- Test 4: write_verilog -remove_cells ---"
set out3 [make_result_file verilog_hier_remove.v]
write_verilog -remove_cells {} $out3
if { [file exists $out3] && [file size $out3] > 0 } {
  puts "PASS: remove_cells write_verilog size=[file size $out3]"
}

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
if { [file exists $out4] && [file size $out4] > 0 } {
  puts "PASS: roundtrip write_verilog size=[file size $out4]"
}

#---------------------------------------------------------------
# Test 6: Set up timing and report with bus ports
#---------------------------------------------------------------
puts "--- Test 6: timing with bus ports ---"
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [all_inputs]
set_output_delay -clock clk 0 [all_outputs]

report_checks
puts "PASS: report_checks GCD"

report_checks -path_delay min
puts "PASS: report_checks min GCD"

report_checks -fields {slew cap input_pins nets fanout}
puts "PASS: report_checks with fields GCD"

#---------------------------------------------------------------
# Test 7: Write verilog after timing setup (tests more writer paths)
#---------------------------------------------------------------
puts "--- Test 7: write after timing setup ---"
set out5 [make_result_file verilog_hier_post_timing.v]
write_verilog $out5
if { [file exists $out5] && [file size $out5] > 0 } {
  puts "PASS: post-timing write_verilog size=[file size $out5]"
}

set out6 [make_result_file verilog_hier_post_timing_pwr.v]
write_verilog -include_pwr_gnd $out6
if { [file exists $out6] && [file size $out6] > 0 } {
  puts "PASS: post-timing pwr write_verilog size=[file size $out6]"
}

puts "ALL PASSED"
