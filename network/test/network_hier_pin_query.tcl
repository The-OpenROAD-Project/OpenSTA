# Test hierarchical pin finding, path name construction, and
# pin type classification (isDriver/isLoad) for deeply nested designs.
# Targets:
#   Network.cc: findPin(path_name), findPinRelative, findPinLinear,
#     pathName(Pin*), pathName(Instance*), pathName(Net*),
#     pathNameFirst, pathNameLast,
#     isDriver, isLoad, isHierarchical, isLeaf, isTopLevelPort,
#     instance(Pin*), net(Pin*), port(Pin*),
#     connectedPinIterator, visitConnectedPins,
#     drivers, pathNameLess, pathNameCmp
#   ConcreteNetwork.cc: findPin, pathName, portName,
#     isDriver, isLoad, name
#   SdcNetwork.cc: findPin, findNet, findInstance delegation

source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog network_hier_test.v
link_design network_hier_test

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {in1 in2 in3}]
set_output_delay -clock clk 0 [get_ports {out1 out2}]
set_input_transition 0.1 [all_inputs]

# Build timing graph
report_checks

#---------------------------------------------------------------
# Test 1: Flat pin queries by hierarchical path
# Exercises: findPin(path_name), pathNameFirst, pathNameLast
#---------------------------------------------------------------
puts "--- Test 1: hierarchical pin queries ---"

# Top-level instance pins
foreach pin_path {buf_in/A buf_in/Z inv1/A inv1/ZN
                  reg1/D reg1/CK reg1/Q
                  buf_out1/A buf_out1/Z
                  buf_out2/A buf_out2/Z} {
  set pin [get_pins $pin_path]
  set dir [get_property $pin direction]
  set fn [get_full_name $pin]
  puts "$pin_path: dir=$dir full_name=$fn"
}

# Hierarchical pins through sub1 and sub2
foreach pin_path {sub1/and_gate/A1 sub1/and_gate/A2 sub1/and_gate/ZN
                  sub1/buf_gate/A sub1/buf_gate/Z
                  sub2/and_gate/A1 sub2/and_gate/A2 sub2/and_gate/ZN
                  sub2/buf_gate/A sub2/buf_gate/Z} {
  set pin [get_pins $pin_path]
  set dir [get_property $pin direction]
  set fn [get_full_name $pin]
  puts "$pin_path: dir=$dir full_name=$fn"
}

#---------------------------------------------------------------
# Test 2: Pin type classification
# Exercises: isDriver, isLoad, isLeaf, isHierarchical, isTopLevelPort
#---------------------------------------------------------------
puts "--- Test 2: pin classification ---"

# Output pins are drivers
foreach pin_path {buf_in/Z inv1/ZN reg1/Q buf_out1/Z buf_out2/Z} {
  set pin [get_pins $pin_path]
  set is_drv [$pin is_driver]
  set is_ld [$pin is_load]
  set is_leaf [$pin is_leaf]
  puts "$pin_path: is_driver=$is_drv is_load=$is_ld is_leaf=$is_leaf"
}

# Input pins are loads
foreach pin_path {buf_in/A inv1/A reg1/D reg1/CK buf_out1/A buf_out2/A} {
  set pin [get_pins $pin_path]
  set is_drv [$pin is_driver]
  set is_ld [$pin is_load]
  set is_leaf [$pin is_leaf]
  puts "$pin_path: is_driver=$is_drv is_load=$is_ld is_leaf=$is_leaf"
}

# Hierarchical sub-block leaf pins
foreach pin_path {sub1/and_gate/A1 sub1/and_gate/ZN sub1/buf_gate/Z
                  sub2/and_gate/A1 sub2/buf_gate/Z} {
  set pin [get_pins $pin_path]
  set is_drv [$pin is_driver]
  set is_ld [$pin is_load]
  puts "$pin_path: is_driver=$is_drv is_load=$is_ld"
}

# Top-level ports
foreach port_name {clk in1 in2 in3 out1 out2} {
  set p [get_ports $port_name]
  set dir [get_property $p direction]
  puts "port $port_name: dir=$dir"
}

#---------------------------------------------------------------
# Test 3: Instance hierarchy queries
# Exercises: pathName(Instance), childIterator, isInside
#---------------------------------------------------------------
puts "--- Test 3: instance hierarchy ---"

# Top-level instances
foreach inst_name {buf_in sub1 sub2 inv1 reg1 buf_out1 buf_out2} {
  set inst [get_cells $inst_name]
  set ref [get_property $inst ref_name]
  set fn [get_full_name $inst]
  puts "inst $inst_name: ref=$ref full_name=$fn"
}

# Hierarchical instances
foreach inst_name {sub1/and_gate sub1/buf_gate sub2/and_gate sub2/buf_gate} {
  set inst [get_cells $inst_name]
  set ref [get_property $inst ref_name]
  set fn [get_full_name $inst]
  puts "inst $inst_name: ref=$ref full_name=$fn"
}

# Instance properties
foreach inst_name {buf_in inv1 reg1 sub1 sub2} {
  set inst [get_cells $inst_name]
  set ref [get_property $inst ref_name]
  puts "$inst_name ref=$ref"
}

#---------------------------------------------------------------
# Test 4: Net queries at different hierarchy levels
# Exercises: pathName(Net*), findNet, highestConnectedNet
#---------------------------------------------------------------
puts "--- Test 4: net hierarchy ---"

foreach net_name {w1 w2 w3 w4 w5} {
  set net [get_nets $net_name]
  set fn [get_full_name $net]
  puts "net $net_name: full_name=$fn"
}

# Hierarchical nets
set hier_nets [get_nets -hierarchical *]
puts "total hierarchical nets: [llength $hier_nets]"

# Nets within sub-blocks
set sub_nets [get_nets -hierarchical sub1/*]
puts "sub1/* nets: [llength $sub_nets]"

set sub2_nets [get_nets -hierarchical sub2/*]
puts "sub2/* nets: [llength $sub2_nets]"

#---------------------------------------------------------------
# Test 5: Connected pin iteration at hierarchy boundary
# Exercises: connectedPinIterator, visitConnectedPins
#---------------------------------------------------------------
puts "--- Test 5: connected pins across hierarchy ---"

foreach net_name {w1 w2 w3 w4 w5} {
  set net [get_nets $net_name]
  set pins_on_net [get_pins -of_objects $net]
  puts "net $net_name has [llength $pins_on_net] pins"
}

# Report nets to exercise detailed connected pin traversal
report_net w1
report_net w2
report_net w3

#---------------------------------------------------------------
# Test 6: Hierarchical pin pattern matching
# Exercises: findPinsMatching, findPinsHierMatching
#---------------------------------------------------------------
puts "--- Test 6: pin pattern matching ---"

set all_pins [get_pins */*]
puts "flat */* pins: [llength $all_pins]"

set hier_pins [get_pins -hierarchical *]
puts "hier * pins: [llength $hier_pins]"

set sub1_pins [get_pins sub1/*]
puts "sub1/* pins: [llength $sub1_pins]"

set hier_sub_pins [get_pins -hierarchical sub*/*]
puts "hier sub*/* pins: [llength $hier_sub_pins]"

set and_pins [get_pins -hierarchical *and*/*]
puts "hier *and*/* pins: [llength $and_pins]"

set buf_pins [get_pins -hierarchical *buf*/*]
puts "hier *buf*/* pins: [llength $buf_pins]"

#---------------------------------------------------------------
# Test 7: Timing through hierarchy
# Exercises: detailed arc traversal through hierarchical pins
#---------------------------------------------------------------
puts "--- Test 7: timing through hierarchy ---"

report_checks -from [get_ports in1] -to [get_ports out1]

report_checks -from [get_ports in2] -to [get_ports out1]

report_checks -from [get_ports in3] -to [get_ports out2]

report_checks -from [get_ports in1] -to [get_ports out2]

# Min path
report_checks -path_delay min -from [get_ports in1] -to [get_ports out1]

#---------------------------------------------------------------
# Test 8: Fanin/fanout through hierarchy
# Exercises: fanin/fanout traversal across hierarchy boundaries
#---------------------------------------------------------------
puts "--- Test 8: fanin/fanout ---"

set fi [get_fanin -to [get_ports out1] -flat]
puts "fanin to out1 flat: [llength $fi]"

set fi_cells [get_fanin -to [get_ports out1] -only_cells]
puts "fanin to out1 cells: [llength $fi_cells]"

set fo [get_fanout -from [get_ports in1] -flat]
puts "fanout from in1 flat: [llength $fo]"

set fo_ep [get_fanout -from [get_ports in1] -endpoints_only]
puts "fanout from in1 endpoints: [llength $fo_ep]"

set fi2 [get_fanin -to [get_ports out2] -flat]
puts "fanin to out2 flat: [llength $fi2]"

set fo2 [get_fanout -from [get_ports in3] -flat]
puts "fanout from in3 flat: [llength $fo2]"
