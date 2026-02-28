# Test hierarchical network operations for coverage improvement.
# Targets: ConcreteNetwork.cc (hierarchical instance/pin/net iteration,
#   childIterator, findChild, findNet in child context, instancePinIterator,
#   instanceNetIterator, cellName, portCount, addConstantNet, clearConstantNets,
#   mergeInto path, netMergedInto, makeCloneInstance, setName)
# Network.cc (findInstanceRelative, pathNameNet, pathNameTerm, pathName for
#   hierarchical pins, connectedNets, highestNetAbove, highestConnectedNet,
#   hierarchyLevel, isInside, isHierarchical, leafInstanceIterator,
#   instanceCount, pinCount, netCount, leafInstanceCount, leafPinCount)
# SdcNetwork.cc (findPin, findNet across hierarchy, escapeDividers,
#   escapeBrackets, portDirection, makePort, deletePort)
# HpinDrvrLoad.cc (visitHpinDrvrLoads through hierarchical pin traversal)

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog network_hier_test.v
link_design network_hier_test

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {in1 in2 in3}]
set_output_delay -clock clk 0 [get_ports {out1 out2}]
set_input_transition 0.1 [get_ports {in1 in2 in3 clk}]

#---------------------------------------------------------------
# Test hierarchical cell queries
# Exercises: findInstancesMatching with hierarchy, childIterator,
#   isHierarchical, isLeaf, findChild
#---------------------------------------------------------------
puts "--- hierarchical cell queries ---"
set all_cells [get_cells *]
puts "flat cells: [llength $all_cells]"

set hier_cells [get_cells -hierarchical *]
puts "hierarchical cells: [llength $hier_cells]"

# Query sub-blocks
set sub_cells [get_cells sub*]
puts "sub* cells (flat): [llength $sub_cells]"

set sub_hier [get_cells -hierarchical sub*]
puts "sub* cells (hier): [llength $sub_hier]"

# Query cells inside sub-blocks
set sub1_cells [get_cells -hierarchical sub1/*]
puts "sub1/* cells (hier): [llength $sub1_cells]"

set sub2_cells [get_cells -hierarchical sub2/*]
puts "sub2/* cells (hier): [llength $sub2_cells]"

# Query all leaf cells hierarchically
set leaf_hier [get_cells -hierarchical *gate*]
puts "*gate* cells (hier): [llength $leaf_hier]"

#---------------------------------------------------------------
# Test hierarchical pin queries
# Exercises: findPinsMatching with hierarchy, hierarchical pin paths,
#   pathName for hierarchical pins
#---------------------------------------------------------------
puts "--- hierarchical pin queries ---"
set all_pins [get_pins */*]
puts "flat pins: [llength $all_pins]"

set hier_pins [get_pins -hierarchical *]
puts "hierarchical all pins: [llength $hier_pins]"

# Query pins inside sub-blocks
set sub1_hier_pins [get_pins -hierarchical sub1/*]
puts "sub1/* pins (hier): [llength $sub1_hier_pins]"

set sub2_hier_pins [get_pins -hierarchical sub2/*]
puts "sub2/* pins (hier): [llength $sub2_hier_pins]"

# Query specific pin paths through hierarchy
set sub1_and [get_pins sub1/and_gate/A1]
puts "sub1/and_gate/A1: [get_full_name $sub1_and]"

# Top-level instance pins (port pins)
set port_pins_in [get_pins -hierarchical */A]
puts "*/A pins (hier): [llength $port_pins_in]"

set port_pins_out [get_pins -hierarchical */Z]
puts "*/Z pins (hier): [llength $port_pins_out]"

set port_pins_zn [get_pins -hierarchical */ZN]
puts "*/ZN pins (hier): [llength $port_pins_zn]"

#---------------------------------------------------------------
# Test hierarchical net queries
# Exercises: findNetsMatching with hierarchy, pathNameNet
#---------------------------------------------------------------
puts "--- hierarchical net queries ---"
set all_nets [get_nets *]
puts "flat nets: [llength $all_nets]"

set hier_nets [get_nets -hierarchical *]
puts "hierarchical nets: [llength $hier_nets]"

set w_nets [get_nets w*]
puts "w* nets (flat): [llength $w_nets]"

set hier_w_nets [get_nets -hierarchical w*]
puts "w* nets (hier): [llength $hier_w_nets]"

#---------------------------------------------------------------
# Test port property queries
# Exercises: portDirection, isInput, isOutput, isBidirect
#---------------------------------------------------------------
puts "--- port properties ---"
set all_ports [get_ports *]
puts "total ports: [llength $all_ports]"

foreach port_name {clk in1 in2 in3 out1 out2} {
  set p [get_ports $port_name]
  set dir [get_property $p direction]
  puts "port $port_name: direction=$dir"
}

# Filter ports by direction
set in_ports [get_ports -filter "direction == input"]
puts "input ports: [llength $in_ports]"

set out_ports [get_ports -filter "direction == output"]
puts "output ports: [llength $out_ports]"

#---------------------------------------------------------------
# Test instance properties across hierarchy
# Exercises: cellName, ref_name, lib_name, is_hierarchical
#---------------------------------------------------------------
puts "--- instance properties ---"
foreach inst_name {buf_in sub1 sub2 inv1 reg1 buf_out1 buf_out2} {
  set inst [get_cells $inst_name]
  set ref [get_property $inst ref_name]
  set full [get_full_name $inst]
  puts "$inst_name: ref=$ref full=$full"
}

#---------------------------------------------------------------
# Test report_instance across hierarchy
# Exercises: instancePinIterator, portCount for each cell
#---------------------------------------------------------------
puts "--- report_instance hierarchy ---"
foreach inst_name {buf_in sub1 sub2 inv1 reg1 buf_out1 buf_out2} {
  report_instance $inst_name
  puts "report_instance $inst_name: done"
}

#---------------------------------------------------------------
# Test report_net for internal hierarchical nets
# Exercises: netPinIterator, net driver/load identification
#---------------------------------------------------------------
puts "--- report_net internal ---"
foreach net_name {w1 w2 w3 w4 w5} {
  report_net $net_name
  puts "report_net $net_name: done"
}

#---------------------------------------------------------------
# Test fanin/fanout traversal through hierarchy
# Exercises: visitDrvrLoadsThruHierPin, HpinDrvrLoad paths
#---------------------------------------------------------------
puts "--- fanin/fanout through hierarchy ---"
set fi_out1 [get_fanin -to [get_ports out1] -flat]
puts "fanin to out1 flat: [llength $fi_out1]"

set fi_out1_cells [get_fanin -to [get_ports out1] -only_cells]
puts "fanin to out1 cells: [llength $fi_out1_cells]"

set fi_out1_start [get_fanin -to [get_ports out1] -startpoints_only]
puts "fanin to out1 startpoints: [llength $fi_out1_start]"

set fo_in1 [get_fanout -from [get_ports in1] -flat]
puts "fanout from in1 flat: [llength $fo_in1]"

set fo_in1_cells [get_fanout -from [get_ports in1] -only_cells]
puts "fanout from in1 cells: [llength $fo_in1_cells]"

set fo_in1_end [get_fanout -from [get_ports in1] -endpoints_only]
puts "fanout from in1 endpoints: [llength $fo_in1_end]"

# Fanin/out with different trace arcs
set fi_timing [get_fanin -to [get_ports out2] -flat -trace_arcs timing]
puts "fanin to out2 timing trace: [llength $fi_timing]"

set fi_all [get_fanin -to [get_ports out2] -flat -trace_arcs all]
puts "fanin to out2 all trace: [llength $fi_all]"

set fo_all [get_fanout -from [get_ports in2] -flat -trace_arcs all]
puts "fanout from in2 all trace: [llength $fo_all]"

# Fanin/out with levels
set fi_lev1 [get_fanin -to [get_ports out1] -flat -levels 1]
puts "fanin to out1 levels=1: [llength $fi_lev1]"

set fi_lev3 [get_fanin -to [get_ports out1] -flat -levels 3]
puts "fanin to out1 levels=3: [llength $fi_lev3]"

set fo_lev1 [get_fanout -from [get_ports in1] -flat -levels 1]
puts "fanout from in1 levels=1: [llength $fo_lev1]"

#---------------------------------------------------------------
# Test timing analysis through hierarchy
# Exercises: graph construction with hierarchical instances
#---------------------------------------------------------------
puts "--- timing through hierarchy ---"
report_checks

report_checks -path_delay min

report_checks -path_delay max

report_checks -from [get_ports in1] -to [get_ports out1]

report_checks -from [get_ports in2] -to [get_ports out1]

report_checks -from [get_ports in3] -to [get_ports out2]

report_checks -fields {slew cap input_pins nets fanout}

report_checks -format full_clock

#---------------------------------------------------------------
# Test network modification in hierarchical context
# Exercises: make_instance, connect_pin, disconnect_pin, delete_instance
#   in presence of hierarchical instances
#---------------------------------------------------------------
puts "--- network modification with hierarchy ---"
set new_buf [make_instance new_hier_buf NangateOpenCellLibrary/BUF_X1]

set new_net [make_net new_hier_net]

connect_pin new_hier_net new_hier_buf/A

disconnect_pin new_hier_net new_hier_buf/A

delete_instance new_hier_buf

delete_net new_hier_net

#---------------------------------------------------------------
# Test all_registers in hierarchical context
#---------------------------------------------------------------
puts "--- registers in hierarchy ---"
set regs [all_registers]
puts "all_registers: [llength $regs]"

set reg_data [all_registers -data_pins]
puts "register data_pins: [llength $reg_data]"

set reg_clk [all_registers -clock_pins]
puts "register clock_pins: [llength $reg_clk]"

set reg_out [all_registers -output_pins]
puts "register output_pins: [llength $reg_out]"

#---------------------------------------------------------------
# Test report_check_types in hierarchical context
#---------------------------------------------------------------
puts "--- report_check_types ---"
report_check_types -max_delay -min_delay -verbose
