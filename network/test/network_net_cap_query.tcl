# Test net capacitance, driver/load pin queries, and net/pin iterator operations.
# Targets:
#   Network.cc: connectedCap, netDriverPins, netLoadPins, netPins,
#     connectedPinIterator, isPower, isGround, highestConnectedNet,
#     isTopLevelPort, isHierarchical (pin), isDriver, isLoad, isLeaf (pin)
#   ConcreteNetwork.cc: netIterator, pinIterator, connectedPinIterator,
#     instance(Net), instance(Pin), net(Pin), port(Pin), term(Pin),
#     isPower, isGround, name(Net), portName(Pin)
#   SdcNetwork.cc: delegation of net/pin queries, capacitance methods

source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog ../../dcalc/test/dcalc_multidriver_test.v
link_design dcalc_multidriver_test

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {in1 in2 in3 in4 sel}]
set_output_delay -clock clk 0 [get_ports {out1 out2 out3}]
set_input_transition 0.1 [get_ports {in1 in2 in3 in4 sel clk}]

#---------------------------------------------------------------
# Test 1: Net driver and load pin queries
# Exercises: netDriverPins, netLoadPins, netPins
#---------------------------------------------------------------
puts "--- Test 1: net driver/load queries ---"

# Query net n1 (driven by buf1/Z, loaded by inv1/A)
set net_n1 [get_nets n1]
puts "net n1 found"

# Get driver pins
set n1_drvr [get_pins -of_objects $net_n1 -filter "direction == output"]
puts "n1 driver pins: [llength $n1_drvr]"

# Get load pins
set n1_load [get_pins -of_objects $net_n1 -filter "direction == input"]
puts "n1 load pins: [llength $n1_load]"

# Query net n6 (driven by or1/ZN, loaded by nand1/A1, nor1/A1, buf_out/A)
set net_n6 [get_nets n6]
puts "net n6 found"

set n6_all_pins [get_pins -of_objects $net_n6]
puts "n6 total pins: [llength $n6_all_pins]"

# report_net exercises pin iteration
report_net n1

report_net n6

# All internal nets
foreach net_name {n1 n2 n3 n4 n5 n6 n7 n8} {
  report_net $net_name
  puts "report_net $net_name: done"
}

#---------------------------------------------------------------
# Test 2: Net capacitance queries
# Exercises: connectedCap, wireCap, pinCap
#---------------------------------------------------------------
puts "--- Test 2: net capacitance queries ---"

# Get capacitance for nets
foreach net_name {n1 n2 n3 n4 n5 n6 n7 n8} {
  set net [get_nets $net_name]
  set net_cap [$net capacitance [sta::cmd_scene] "max"]
  set pin_cap [$net pin_capacitance [sta::cmd_scene] "max"]
  set wire_cap [$net wire_capacitance [sta::cmd_scene] "max"]
  puts "$net_name: total_cap=$net_cap pin_cap=$pin_cap wire_cap=$wire_cap"
}

#---------------------------------------------------------------
# Test 3: Pin property queries
# Exercises: isDriver, isLoad, isLeaf, isHierarchical, isTopLevelPort,
#   portName, instance, net, port, term
#---------------------------------------------------------------
puts "--- Test 3: pin property queries ---"

# Port pins (use get_port_pin via port object)
foreach port_name {in1 in2 out1 out2 out3 clk} {
  set port [get_ports $port_name]
  set pin [sta::get_port_pin $port]
  set is_driver [$pin is_driver]
  set is_load [$pin is_load]
  set is_leaf [$pin is_leaf]
  set is_top [$pin is_top_level_port]
  puts "$port_name: driver=$is_driver load=$is_load leaf=$is_leaf top=$is_top"
}

# Instance pins
foreach pin_path {buf1/A buf1/Z inv1/A inv1/ZN and1/A1 and1/ZN or1/A1 or1/ZN reg1/D reg1/CK reg1/Q} {
  set pin [get_pins $pin_path]
  set is_driver [$pin is_driver]
  set is_load [$pin is_load]
  set is_leaf [$pin is_leaf]
  set is_top [$pin is_top_level_port]
  set port_name [$pin port_name]
  puts "$pin_path: driver=$is_driver load=$is_load leaf=$is_leaf top=$is_top port=$port_name"
}

#---------------------------------------------------------------
# Test 4: Pin connected pin iterator
# Exercises: connectedPinIterator
#---------------------------------------------------------------
puts "--- Test 4: connected pin queries ---"

# Connected pins from a driver
set buf1_z [get_pins buf1/Z]
set conn_iter [$buf1_z connected_pin_iterator]
set conn_count 0
while {[$conn_iter has_next]} {
  set cpin [$conn_iter next]
  incr conn_count
}
$conn_iter finish
puts "buf1/Z connected pins: $conn_count"

# Connected pins from a load
set inv1_a [get_pins inv1/A]
set conn_iter2 [$inv1_a connected_pin_iterator]
set conn_count2 0
while {[$conn_iter2 has_next]} {
  set cpin [$conn_iter2 next]
  incr conn_count2
}
$conn_iter2 finish
puts "inv1/A connected pins: $conn_count2"

# Connected pins from multi-fanout net driver
set or1_zn [get_pins or1/ZN]
set conn_iter3 [$or1_zn connected_pin_iterator]
set conn_count3 0
while {[$conn_iter3 has_next]} {
  set cpin [$conn_iter3 next]
  incr conn_count3
}
$conn_iter3 finish
puts "or1/ZN connected pins: $conn_count3"

#---------------------------------------------------------------
# Test 5: Instance pin/net iterators
# Exercises: instancePinIterator, instanceNetIterator
#---------------------------------------------------------------
puts "--- Test 5: instance iterators ---"

# Top instance
set top [sta::top_instance]
set child_iter [$top child_iterator]
set child_count 0
while {[$child_iter has_next]} {
  set child [$child_iter next]
  incr child_count
}
$child_iter finish
puts "top child count: $child_count"

# Pin iterator on an instance
set reg1 [get_cells reg1]
set pin_iter [$reg1 pin_iterator]
set pin_count 0
while {[$pin_iter has_next]} {
  set pin [$pin_iter next]
  incr pin_count
}
$pin_iter finish
puts "reg1 pin count: $pin_count"

# Net iterator on top instance
set net_iter [$top net_iterator]
set net_count 0
while {[$net_iter has_next]} {
  set net [$net_iter next]
  incr net_count
}
$net_iter finish
puts "top net count: $net_count"

#---------------------------------------------------------------
# Test 6: Network counting functions
# Exercises: instanceCount, pinCount, netCount, leafInstanceCount, leafPinCount
#---------------------------------------------------------------
puts "--- Test 6: network counts ---"

set inst_count [sta::network_instance_count]
puts "instance count: $inst_count"

set pin_count [sta::network_pin_count]
puts "pin count: $pin_count"

set net_count [sta::network_net_count]
puts "net count: $net_count"

set leaf_inst [sta::network_leaf_instance_count]
puts "leaf instance count: $leaf_inst"

set leaf_pin [sta::network_leaf_pin_count]
puts "leaf pin count: $leaf_pin"

#---------------------------------------------------------------
# Test 7: Leaf instance iterator
# Exercises: leafInstanceIterator, leafInstances
#---------------------------------------------------------------
puts "--- Test 7: leaf instance iterator ---"

set leaf_iter [sta::leaf_instance_iterator]
set leaf_count 0
while {[$leaf_iter has_next]} {
  set inst [$leaf_iter next]
  incr leaf_count
}
$leaf_iter finish
puts "leaf instance count via iterator: $leaf_count"

set leaf_insts [sta::network_leaf_instances]
puts "leaf instances list: [llength $leaf_insts]"

#---------------------------------------------------------------
# Test 8: Library queries
# Exercises: libraryIterator, findLibrary, library_name
#---------------------------------------------------------------
puts "--- Test 8: library queries ---"

set lib_iter [sta::library_iterator]
set lib_count 0
while {[$lib_iter has_next]} {
  set lib [$lib_iter next]
  set lib_name [$lib name]
  puts "library: $lib_name"
  incr lib_count
}
$lib_iter finish
puts "total libraries: $lib_count"

# Find library by name
set found_lib [sta::find_library NangateOpenCellLibrary]
if { $found_lib != "NULL" } {
  puts "found library: [$found_lib name]"
} else {
  error "expected NangateOpenCellLibrary to exist"
}

# Find cell in library
set inv_cell [$found_lib find_cell INV_X1]
if { $inv_cell != "NULL" } {
  puts "found INV_X1 in NangateOpenCellLibrary"
} else {
  error "expected INV_X1 cell in NangateOpenCellLibrary"
}

# find_cells_matching on library
set buf_cells [$found_lib find_cells_matching "BUF*" 0 0]
puts "BUF* cells in library: [llength $buf_cells]"

set star_cells [$found_lib find_cells_matching "*" 0 0]
puts "all cells in library: [llength $star_cells]"

#---------------------------------------------------------------
# Test 9: Report with various output formats
# Exercises: timing paths using net/pin property queries
#---------------------------------------------------------------
puts "--- Test 9: timing reports ---"

report_checks

report_checks -path_delay min

report_checks -fields {slew cap input_pins fanout}

# Check types
report_check_types -max_delay -min_delay

report_check_types -max_slew -max_capacitance -max_fanout
