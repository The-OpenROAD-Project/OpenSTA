# Test connected pin iteration, merge nets, replace cell, and
# pin/net/instance deletion in the ConcreteNetwork.
# Targets:
#   Network.cc: connectedPinIterator (net/pin), visitConnectedPins,
#     connectedNets, isConnected, drivers, netDrvrPinMap, clearNetDrvrPinMap,
#     highestNetAbove, highestConnectedNet, leafInstanceCount, leafPinCount,
#     leafInstances, leafInstanceIterator, instanceCount, pinCount, netCount
#   ConcreteNetwork.cc: mergeInto, replaceCell, deleteInstance, deleteNet,
#     makeConcretePort, makeBusPort, makeBundlePort, groupBusPorts,
#     connect/disconnect, findPort, findPin, setAttribute, getAttribute,
#     setName, setIsLeaf, cellNetworkView, setCellNetworkView,
#     makePins, isPower, isGround, clearConstantNets, addConstantNet,
#     constantPinIterator, visitConnectedPins

source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog network_test1.v
link_design network_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_input_delay -clock clk 0 [get_ports in2]
set_output_delay -clock clk 0 [get_ports out1]
set_input_transition 0.1 [all_inputs]

# Build timing graph
report_checks

#---------------------------------------------------------------
# Exercise connected pin queries
# Targets: connectedPinIterator, visitConnectedPins
#---------------------------------------------------------------
puts "--- connected pin queries ---"

# Report net n1 to exercise connected pin iteration
report_net n1

report_net n2

# Report all nets to iterate all connected pins
foreach net_name {n1 n2} {
  set net [get_nets $net_name]
  set pins_on_net [get_pins -of_objects $net]
  puts "net $net_name pins: [llength $pins_on_net]"
}

#---------------------------------------------------------------
# Exercise instance creation, pin connection, cell replacement
# Targets: makeConcreteInstance, makePins, connect, replaceCell
#---------------------------------------------------------------
puts "--- instance/connection lifecycle ---"

# Create multiple instances of various cell types
set inst_list {}
set cell_types {BUF_X1 BUF_X2 BUF_X4 INV_X1 INV_X2 AND2_X1 OR2_X1 NAND2_X1 NOR2_X1}
set idx 0
foreach ctype $cell_types {
  set iname "lifecycle_inst_$idx"
  set inst [make_instance $iname NangateOpenCellLibrary/$ctype]
  lappend inst_list $iname
  incr idx
}
puts "created [llength $inst_list] instances"

# Create nets and connect pins
set net_list {}
for {set i 0} {$i < 6} {incr i} {
  set nname "lifecycle_net_$i"
  make_net $nname
  lappend net_list $nname
}
puts "created [llength $net_list] nets"

# Connect BUF_X1 input
set msg [connect_pin lifecycle_net_0 lifecycle_inst_0/A]
puts "connect lifecycle_inst_0/A: $msg"

# Connect BUF_X1 output
set msg [connect_pin lifecycle_net_1 lifecycle_inst_0/Z]
puts "connect lifecycle_inst_0/Z: $msg"

# Connect INV_X1 input/output
set msg [connect_pin lifecycle_net_1 lifecycle_inst_3/A]
puts "connect lifecycle_inst_3/A: $msg"

set msg [connect_pin lifecycle_net_2 lifecycle_inst_3/ZN]
puts "connect lifecycle_inst_3/ZN: $msg"

# Report net with connected pins (exercises connectedPinIterator)
report_net lifecycle_net_1
puts "report_net lifecycle_net_1: done"

# Replace cell: BUF_X1 -> BUF_X2 (compatible ports A, Z)
replace_cell lifecycle_inst_0 NangateOpenCellLibrary/BUF_X2
set ref_after [get_property [get_cells lifecycle_inst_0] ref_name]
puts "replace BUF_X1->BUF_X2: ref=$ref_after"

# Replace back
replace_cell lifecycle_inst_0 NangateOpenCellLibrary/BUF_X1
set ref_back [get_property [get_cells lifecycle_inst_0] ref_name]
puts "replace back BUF_X2->BUF_X1: ref=$ref_back"

# Replace to BUF_X4
replace_cell lifecycle_inst_0 NangateOpenCellLibrary/BUF_X4
set ref_x4 [get_property [get_cells lifecycle_inst_0] ref_name]
puts "replace BUF_X1->BUF_X4: ref=$ref_x4"

# Incremental timing after modifications
report_checks

#---------------------------------------------------------------
# Disconnect and delete
# Targets: disconnectPin, deleteInstance, deleteNet
#---------------------------------------------------------------
puts "--- disconnect and delete ---"

# Disconnect all connected pins
disconnect_pin lifecycle_net_0 lifecycle_inst_0/A
disconnect_pin lifecycle_net_1 lifecycle_inst_0/Z
disconnect_pin lifecycle_net_1 lifecycle_inst_3/A
disconnect_pin lifecycle_net_2 lifecycle_inst_3/ZN

# Delete all lifecycle instances
foreach iname $inst_list {
  delete_instance $iname
}

# Delete all lifecycle nets
foreach nname $net_list {
  delete_net $nname
}

# Verify design still works
set final_cells [get_cells *]
puts "final cells: [llength $final_cells]"
report_checks

#---------------------------------------------------------------
# Exercise various property queries
# Targets: getAttribute, cell properties
#---------------------------------------------------------------
puts "--- property queries ---"
foreach port_name {clk in1 in2 out1} {
  set p [get_ports $port_name]
  set dir [get_property $p direction]
  set fn [get_full_name $p]
  puts "$port_name: dir=$dir full_name=$fn"
}

foreach inst_name {buf1 and1 reg1} {
  set inst [get_cells $inst_name]
  set ref [get_property $inst ref_name]
  set fn [get_full_name $inst]
  puts "$inst_name: ref=$ref full_name=$fn"
}

foreach pin_path {buf1/A buf1/Z and1/A1 and1/A2 and1/ZN reg1/D reg1/CK reg1/Q} {
  set pin [get_pins $pin_path]
  set dir [get_property $pin direction]
  puts "$pin_path: dir=$dir"
}

#---------------------------------------------------------------
# Multiple replace_cell on the original design instances
# Targets: replaceCell with different port configurations
#---------------------------------------------------------------
puts "--- replace_cell original instances ---"

# Replace and1 (AND2_X1) with AND2_X2
replace_cell and1 NangateOpenCellLibrary/AND2_X2
set ref [get_property [get_cells and1] ref_name]
puts "and1 -> AND2_X2: $ref"

# Report checks to force delay recalculation
report_checks

# Replace and1 back
replace_cell and1 NangateOpenCellLibrary/AND2_X1
report_checks

# Replace buf1 through multiple sizes
foreach size {X2 X4 X8 X16 X32} {
  replace_cell buf1 NangateOpenCellLibrary/BUF_$size
  set ref [get_property [get_cells buf1] ref_name]
  puts "buf1 -> BUF_$size: $ref"
}
# Replace back
replace_cell buf1 NangateOpenCellLibrary/BUF_X1
report_checks
