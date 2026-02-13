# Read sky130hd liberty and query cells/pins
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib

set lib [get_libs sky130_fd_sc_hd__tt_025C_1v80]
if { $lib == "" } {
  puts "FAIL: library not found"
  exit 1
}
puts "PASS: library loaded"

# Query a common cell
set cell [get_lib_cells sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__inv_1]
if { $cell == "" } {
  puts "FAIL: inv_1 cell not found"
  exit 1
}
puts "PASS: inv_1 cell found"

# Query pins
set pins [get_lib_pins sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__inv_1/*]
if { [llength $pins] == 0 } {
  puts "FAIL: no pins found"
  exit 1
}
puts "PASS: pins found ([llength $pins] pins)"

# Query a 2-input gate
set nand [get_lib_cells sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__nand2_1]
if { $nand == "" } {
  puts "FAIL: nand2_1 not found"
  exit 1
}
puts "PASS: nand2_1 found"

puts "ALL PASSED"
