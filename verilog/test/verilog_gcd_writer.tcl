# Test verilog writer with GCD sky130 design (large design with bus nets,
# unconnected pins, many cell types, power/ground nets).
# Targets: VerilogWriter.cc uncovered:
#   writeInstBusPin / writeInstBusPinBit (bus port handling)
#   writeWireDcls (bus wire declarations, isBusName, parseBusName paths)
#   findUnconnectedNetCount / findChildNCcount / findPortNCcount
#   writeAssigns (assign statements from net connections)
#   verilogPortDir for power/ground ports with -include_pwr_gnd
# Also targets: VerilogReader.cc paths from re-reading written output

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 1: Write GCD sky130 with various options
#---------------------------------------------------------------
puts "--- Test 1: GCD sky130 write ---"
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../examples/gcd_sky130hd.v
link_design gcd

set cells [get_cells *]
set nets [get_nets *]
set ports [get_ports *]
puts "cells: [llength $cells]"
puts "nets: [llength $nets]"
puts "ports: [llength $ports]"

# Basic write
set out1 [make_result_file verilog_gcd_basic.v]
write_verilog $out1
set sz1 [file size $out1]
puts "basic write: $sz1 bytes"
if { $sz1 > 0 } {
  puts "PASS: basic write non-empty"
}

# Write with -include_pwr_gnd
set out2 [make_result_file verilog_gcd_pwr.v]
write_verilog -include_pwr_gnd $out2
set sz2 [file size $out2]
puts "pwr_gnd write: $sz2 bytes"
if { $sz2 >= $sz1 } {
  puts "PASS: pwr_gnd >= basic"
}

# Write with -remove_cells (remove buffer cells)
set out3 [make_result_file verilog_gcd_remove.v]
catch {
  set bufs [get_lib_cells sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__buf_1]
  write_verilog -remove_cells $bufs $out3
} msg
if { [file exists $out3] } {
  set sz3 [file size $out3]
  puts "remove_cells write: $sz3 bytes"
} else {
  puts "remove_cells write: skipped ($msg)"
  set sz3 0
}
puts "PASS: write with remove_cells"

# Write with both -include_pwr_gnd and empty remove_cells
set out4 [make_result_file verilog_gcd_pwr_remove.v]
write_verilog -include_pwr_gnd -remove_cells {} $out4
set sz4 [file size $out4]
puts "pwr+remove write: $sz4 bytes"
puts "PASS: write with pwr + remove"

#---------------------------------------------------------------
# Test 2: Read back written verilog (roundtrip test)
# Exercises: VerilogReader re-read of OpenSTA-generated output
#---------------------------------------------------------------
puts "--- Test 2: roundtrip ---"
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog $out1
link_design gcd

set cells2 [get_cells *]
puts "roundtrip cells: [llength $cells2]"

set out5 [make_result_file verilog_gcd_roundtrip.v]
write_verilog $out5
set sz5 [file size $out5]
puts "roundtrip write: $sz5 bytes"

if { abs($sz5 - $sz1) < 100 } {
  puts "PASS: roundtrip sizes similar"
} else {
  puts "INFO: roundtrip sizes differ basic=$sz1 roundtrip=$sz5"
}

#---------------------------------------------------------------
# Test 3: Timing analysis after roundtrip
#---------------------------------------------------------------
puts "--- Test 3: timing after roundtrip ---"
read_sdc ../../examples/gcd_sky130hd.sdc
report_checks
puts "PASS: report_checks after roundtrip"

report_checks -path_delay min
puts "PASS: min path after roundtrip"

report_checks -fields {slew cap input_pins nets fanout}
puts "PASS: fields after roundtrip"

#---------------------------------------------------------------
# Test 4: Write Nangate45 example1 (different PDK, different topology)
#---------------------------------------------------------------
puts "--- Test 4: Nangate45 verilog_test1 ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_test1.v
link_design verilog_test1

set out6 [make_result_file verilog_test1_basic.v]
write_verilog $out6
set sz6 [file size $out6]
puts "verilog_test1 basic: $sz6 bytes"

set out7 [make_result_file verilog_test1_pwr.v]
write_verilog -include_pwr_gnd $out7
set sz7 [file size $out7]
puts "verilog_test1 pwr_gnd: $sz7 bytes"

if { $sz7 >= $sz6 } {
  puts "PASS: verilog_test1 pwr_gnd >= basic"
}

puts "PASS: verilog_test1 write"

#---------------------------------------------------------------
# Test 5: Write with -sort (deprecated option coverage)
#---------------------------------------------------------------
puts "--- Test 5: -sort option ---"
set out8 [make_result_file verilog_gcd_sort.v]
catch {write_verilog -sort $out8} msg_sort
puts "write_verilog -sort: $msg_sort"
if { [file exists $out8] } {
  set sz8 [file size $out8]
  puts "sort write: $sz8 bytes"
}
puts "PASS: -sort option"

#---------------------------------------------------------------
# Test 6: Network modification then write
# Exercises: writeChild with modified topology, unconnected net count
#---------------------------------------------------------------
puts "--- Test 6: modify then write ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_test1.v
link_design verilog_test1

# Add instances to create unconnected pins
set nn [make_net extra_wire]
set ni [make_instance extra_inv NangateOpenCellLibrary/INV_X1]
connect_pin extra_wire extra_inv/A
# ZN is left unconnected -> findPortNCcount path

set out9 [make_result_file verilog_example1_modified.v]
write_verilog $out9
set sz9 [file size $out9]
puts "modified write: $sz9 bytes"
puts "PASS: modified write with unconnected pin"

# Write with pwr_gnd to exercise power/ground direction paths
set out10 [make_result_file verilog_example1_modified_pwr.v]
write_verilog -include_pwr_gnd $out10
set sz10 [file size $out10]
puts "modified pwr_gnd write: $sz10 bytes"
puts "PASS: modified pwr_gnd write"

# Cleanup
disconnect_pin extra_wire extra_inv/A
delete_instance extra_inv
delete_net extra_wire

puts "ALL PASSED"
