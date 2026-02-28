# Test 3: Multiple re-reads of same file
# Exercises: module re-definition paths in VerilogReader
source ../../test/helpers.tcl
suppress_msg 1140

#---------------------------------------------------------------
# Test 3: Multiple re-reads of same file
# Exercises: module re-definition paths in VerilogReader
#---------------------------------------------------------------
puts "--- Test 3: multiple re-reads ---"

# Read same file multiple times
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_test1.v
read_verilog verilog_test1.v
link_design verilog_test1

set cells_rr [get_cells *]
puts "re-read cells: [llength $cells_rr]"

set nets_rr [get_nets *]
puts "re-read nets: [llength $nets_rr]"

# Read different file then same file
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_bus_test.v
read_verilog verilog_test1.v
link_design verilog_test1

set cells_rr2 [get_cells *]
puts "re-read2 cells: [llength $cells_rr2]"

# Read same bus file multiple times
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_bus_test.v
read_verilog verilog_bus_test.v
read_verilog verilog_bus_test.v
link_design verilog_bus_test

set cells_rr3 [get_cells *]
puts "re-read3 bus cells: [llength $cells_rr3]"
