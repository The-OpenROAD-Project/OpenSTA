# Test network modification commands
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog network_test1.v
link_design network_test1

puts "--- initial cell count ---"
set cells [get_cells *]
puts "initial cells: [llength $cells]"

puts "--- current_design ---"
set design [current_design]
puts "current_design: $design"

puts "--- make_net new_net1 ---"
set new_net [make_net new_net1]
if { $new_net != 0 } {
} else {
  puts "FAIL: make_net returned 0"
}

puts "--- verify new net exists ---"
set found_net [get_nets new_net1]
if { [llength $found_net] > 0 } {
} else {
  puts "FAIL: new_net1 not found"
}

puts "--- make_instance new_buf BUF_X1 ---"
set new_inst [make_instance new_buf NangateOpenCellLibrary/BUF_X1]
if { $new_inst != 0 } {
} else {
  puts "FAIL: make_instance returned 0"
}

puts "--- verify new instance exists ---"
set found_inst [get_cells new_buf]
if { [llength $found_inst] > 0 } {
} else {
  puts "FAIL: new_buf not found"
}

puts "--- cell count after adding instance ---"
set cells [get_cells *]
puts "cells after make_instance: [llength $cells]"

puts "--- connect_pin new_net1 new_buf/A ---"
set rc [connect_pin new_net1 new_buf/A]
puts "connect_pin result: $rc"

puts "--- connect output pin to new net ---"
set new_net2 [make_net new_net2]
set rc2 [connect_pin new_net2 new_buf/Z]
puts "connect_pin output result: $rc2"

puts "--- disconnect_pin new_net1 new_buf/A ---"
set rc_disc [disconnect_pin new_net1 new_buf/A]
puts "disconnect_pin result: $rc_disc"

puts "--- replace_cell new_buf with INV_X1 ---"
# INV_X1 has A and ZN, different ports than BUF_X1 (A and Z)
# This should fail because ports don't match
set rc3 [replace_cell new_buf NangateOpenCellLibrary/INV_X1]
puts "replace_cell INV_X1 result (expect 0): $rc3"

puts "--- replace_cell buf1 with BUF_X2 ---"
set rc4 [replace_cell buf1 NangateOpenCellLibrary/BUF_X2]
puts "replace_cell BUF_X2 result: $rc4"

puts "--- verify buf1 cell changed ---"
set buf1_inst [get_cells buf1]
set ref [get_property $buf1_inst ref_name]
puts "buf1 ref_name after replace: $ref"

puts "--- disconnect remaining new_buf pins ---"
catch {disconnect_pin new_net2 new_buf/Z} msg_disc2
puts "disconnect new_buf/Z: $msg_disc2"

puts "--- delete_instance new_buf ---"
delete_instance new_buf

puts "--- verify new_buf removed ---"
set rc5 [catch {get_cells new_buf} msg]
puts "get_cells new_buf after delete: $msg"

puts "--- cell count after delete ---"
set cells [get_cells *]
puts "cells after delete_instance: [llength $cells]"

puts "--- delete_net new_net1 ---"
delete_net new_net1

puts "--- delete_net new_net2 ---"
delete_net new_net2

puts "--- make another instance and delete by object ---"
set inst2 [make_instance temp_buf NangateOpenCellLibrary/BUF_X1]
if { $inst2 != 0 } {
  delete_instance $inst2
} else {
  puts "FAIL: make_instance temp_buf"
}

puts "--- make and delete net by object ---"
set net3 [make_net temp_net]
if { $net3 != 0 } {
  delete_net $net3
} else {
  puts "FAIL: make_net temp_net"
}

puts "--- current_design with name ---"
set design2 [current_design network_test1]
puts "current_design with name: $design2"
