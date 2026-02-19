# Test network comparison and sorting operations
# Targets: NetworkCmp.cc uncovered functions:
#   InstancePathNameLess (line 71-81, hit=0)
#   NetPathNameLess::operator() (line 65-68, hit=0)
#   PortNameLess::operator() (line 41-44, hit=0)
#   sortByPathName(InstanceSet) (line 108-116, hit=0)
#   sortByPathName(NetSet) (line 119-127, hit=0)
# Also targets: Network.cc
#   pathNameLess(Instance) (line 287, hit=0), pathNameCmp(Instance) (line 294)
#   pathNameLess(Net) (line 486, hit=0), pathNameCmp(Net) (line 493, hit=0)
#   instanceCount, pinCount, netCount, leafInstanceCount, leafPinCount
#   setPathDivider (line 1213, hit=0), setPathEscape (line 1219, hit=0)
#   leafInstanceIterator(Instance) (line 1297, hit=0)

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog network_test1.v
link_design network_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_input_delay -clock clk 0 [get_ports in2]
set_output_delay -clock clk 0 [get_ports out1]
set_input_transition 10 {in1 in2 clk}

# Build timing graph
report_checks

#---------------------------------------------------------------
# Test sorting of instances by getting all cells and printing sorted
# Exercises InstancePathNameLess, sortByPathName(InstanceSet)
#---------------------------------------------------------------
puts "--- instance sorting ---"
set all_cells [get_cells *]
puts "total cells: [llength $all_cells]"

# Print all cells - they should be sorted by path name
puts "cells in order:"
foreach c $all_cells {
  puts "  [get_full_name $c] ref=[get_property $c ref_name]"
}

#---------------------------------------------------------------
# Test sorting of nets by getting all nets and printing sorted
# Exercises NetPathNameLess, sortByPathName(NetSet)
#---------------------------------------------------------------
puts "--- net sorting ---"
set all_nets [get_nets *]
puts "total nets: [llength $all_nets]"

puts "nets in order:"
foreach n $all_nets {
  puts "  [get_full_name $n]"
}

#---------------------------------------------------------------
# Test sorting of ports
# Exercises PortNameLess
#---------------------------------------------------------------
puts "--- port sorting ---"
set all_ports [get_ports *]
puts "total ports: [llength $all_ports]"

puts "ports in order:"
foreach p $all_ports {
  puts "  [get_full_name $p] dir=[get_property $p direction]"
}

#---------------------------------------------------------------
# Test sorting of pins
# Exercises PinPathNameLess
#---------------------------------------------------------------
puts "--- pin sorting ---"
set all_pins [get_pins */*]
puts "total pins: [llength $all_pins]"

puts "pins in order:"
foreach p $all_pins {
  puts "  [get_full_name $p] dir=[get_property $p direction]"
}

#---------------------------------------------------------------
# Test collection operations on sorted objects
# Exercises set operations with comparators
#---------------------------------------------------------------
puts "--- collection operations ---"

# Get overlapping sets and check sizes
set buf_pins [get_pins buf1/*]
puts "buf1 pins: [llength $buf_pins]"

set and_pins [get_pins and1/*]
puts "and1 pins: [llength $and_pins]"

set reg_pins [get_pins reg1/*]
puts "reg1 pins: [llength $reg_pins]"

#---------------------------------------------------------------
# Timing analysis with various reporting options to exercise
# sorting in report paths
#---------------------------------------------------------------
puts "--- timing report sorting ---"
report_checks -path_delay max -fields {slew cap input_pins nets fanout}
report_checks -path_delay min -fields {slew cap input_pins nets fanout}
report_checks -sort_by_slack
report_checks -group_path_count 5

#---------------------------------------------------------------
# Test report_net with sorted pin output
#---------------------------------------------------------------
puts "--- report_net sorted ---"
report_net n1
report_net n2

# Report with different digit precision
report_net -digits 2 n1
report_net -digits 4 n2
report_net -digits 6 n1

#---------------------------------------------------------------
# Test report_instance for cells in sorted order
#---------------------------------------------------------------
puts "--- report_instance sorted ---"
foreach inst_name [lsort {buf1 and1 reg1}] {
  report_instance $inst_name
}

#---------------------------------------------------------------
# Test get_property on various object types
# Exercises various name/comparison functions
#---------------------------------------------------------------
puts "--- property queries ---"
set buf1 [get_cells buf1]
puts "buf1 full_name: [get_full_name $buf1]"
puts "buf1 ref_name: [get_property $buf1 ref_name]"

set n1 [get_nets n1]
puts "n1 full_name: [get_full_name $n1]"

set buf1_A [get_pins buf1/A]
puts "buf1/A full_name: [get_full_name $buf1_A]"
puts "buf1/A direction: [get_property $buf1_A direction]"

set in1_port [get_ports in1]
puts "in1 full_name: [get_full_name $in1_port]"
puts "in1 direction: [get_property $in1_port direction]"

#---------------------------------------------------------------
# Test library queries with sorting
#---------------------------------------------------------------
puts "--- library queries ---"
set libs [get_libs *]
puts "libraries: [llength $libs]"

foreach lib $libs {
  puts "  lib: [get_name $lib]"
}
