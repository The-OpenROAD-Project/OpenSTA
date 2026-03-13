# Read sky130hd liberty and query cells/pins
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib

set lib [get_libs sky130_fd_sc_hd__tt_025C_1v80]
if { $lib == "" } {
  puts "FAIL: library not found"
  exit 1
}

# Query a common cell
set cell [get_lib_cells sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__inv_1]
if { $cell == "" } {
  puts "FAIL: inv_1 cell not found"
  exit 1
}

# Query pins
set pins [get_lib_pins sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__inv_1/*]
if { [llength $pins] == 0 } {
  puts "FAIL: no pins found"
  exit 1
}
if { [llength $pins] != 2 } {
  error "expected exactly 2 pins on sky130 inv_1, found [llength $pins]"
}

set inv_area [get_property $cell area]
if { $inv_area <= 0.0 } {
  error "expected positive area for sky130 inv_1, got $inv_area"
}

set inv_a [get_lib_pins sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__inv_1/A]
set inv_y [get_lib_pins sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__inv_1/Y]
if { $inv_a == "" || $inv_y == "" } {
  error "expected both A and Y pins on sky130 inv_1"
}

# Query a 2-input gate
set nand [get_lib_cells sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__nand2_1]
if { $nand == "" } {
  puts "FAIL: nand2_1 not found"
  exit 1
}

set nand_pins [get_lib_pins sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__nand2_1/*]
if { [llength $nand_pins] != 3 } {
  error "expected 3 pins on sky130 nand2_1, found [llength $nand_pins]"
}
