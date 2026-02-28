# Test leaf instance iteration, path name operations, and
# network count/traversal functions.
# Targets:
#   Network.cc: leafInstanceIterator, isInside, isHierarchical,
#     pathName (instance, pin, net), pathNameCmp, pathNameLess,
#     portName, isTopInstance, path, findLibertyCell,
#     findLibertyFilename, busIndexInRange, hasMembers,
#     findInstancesMatching, findPinsMatching, findNetsMatching,
#     findPortsMatching (bus and non-bus paths),
#     checkLibertyCorners, checkNetworkLibertyCorners
#   ConcreteNetwork.cc: findAnyCell, deleteLibrary,
#     makeInstance(LibertyCell,...), libertyCell(Cell*),
#     libertyCell(Instance*), cell(LibertyCell*),
#     filename(Cell*), isLeaf(Cell*), setAttribute/getAttribute,
#     netIterator, childIterator, pinIterator, termIterator
#   SdcNetwork.cc: findPort, findPin, findNet with dividers,
#     escapeDividers, escapeBrackets, portDirection
#   ParseBus.cc: parseBusName edge cases

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog network_hier_test.v
link_design network_hier_test

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {in1 in2 in3}]
set_output_delay -clock clk 0 [get_ports {out1 out2}]
set_input_transition 0.1 [all_inputs]

# Build the timing graph
report_checks

#---------------------------------------------------------------
# Leaf instance queries
# Exercises: leafInstanceIterator, isLeaf, isHierarchical
#---------------------------------------------------------------
puts "--- leaf instance queries ---"

# Get all flat cells (leaf instances)
set all_flat [get_cells *]
puts "flat cells: [llength $all_flat]"

# Get all hierarchical cells
set all_hier [get_cells -hierarchical *]
puts "hierarchical cells: [llength $all_hier]"

# Check specific instances
foreach inst_name {buf_in sub1 sub2 inv1 reg1 buf_out1 buf_out2} {
  set inst [get_cells $inst_name]
  set ref [get_property $inst ref_name]
  set fn [get_full_name $inst]
  puts "inst $inst_name: ref=$ref full=$fn"
}

#---------------------------------------------------------------
# Test hierarchical instance path traversal
# Exercises: path, isTopInstance, pathName
#---------------------------------------------------------------
puts "--- path traversal ---"

# Query deep instances through hierarchy
set deep_cells [get_cells -hierarchical *gate*]
puts "deep *gate* cells: [llength $deep_cells]"

# Instance names at various depths
foreach cell $all_hier {
  set fn [get_full_name $cell]
  set ref [get_property $cell ref_name]
  puts "  hier: $fn ref=$ref"
}

#---------------------------------------------------------------
# Test port queries and bus handling
# Exercises: findPortsMatching, portName, direction
#---------------------------------------------------------------
puts "--- port queries ---"

set all_ports [get_ports *]
puts "total ports: [llength $all_ports]"

# Input filter
set in_ports [get_ports -filter "direction == input"]
puts "input ports: [llength $in_ports]"

# Output filter
set out_ports [get_ports -filter "direction == output"]
puts "output ports: [llength $out_ports]"

# Individual port properties
foreach port_name {clk in1 in2 in3 out1 out2} {
  set p [get_ports $port_name]
  set dir [get_property $p direction]
  set fn [get_full_name $p]
  puts "port $port_name: dir=$dir name=$fn"
}

#---------------------------------------------------------------
# Test pin queries at various levels
# Exercises: findPinsMatching (flat and hierarchical)
#---------------------------------------------------------------
puts "--- pin queries ---"

set all_pins [get_pins */*]
puts "flat pins: [llength $all_pins]"

set hier_pins [get_pins -hierarchical *]
puts "all hierarchical pins: [llength $hier_pins]"

# Specific pin patterns
set a_pins [get_pins */A]
puts "*/A pins: [llength $a_pins]"

set z_pins [get_pins */Z]
puts "*/Z pins: [llength $z_pins]"

set zn_pins [get_pins */ZN]
puts "*/ZN pins: [llength $zn_pins]"

set ck_pins [get_pins */CK]
puts "*/CK pins: [llength $ck_pins]"

# Deep hierarchical pin queries
set sub_pins [get_pins -hierarchical sub*/*]
puts "sub*/* hier pins: [llength $sub_pins]"

# All pins within subblocks
set sub1_pins [get_pins -hierarchical sub1/*]
puts "sub1/* hier pins: [llength $sub1_pins]"

set sub2_pins [get_pins -hierarchical sub2/*]
puts "sub2/* hier pins: [llength $sub2_pins]"

#---------------------------------------------------------------
# Test net queries at various levels
# Exercises: findNetsMatching (flat and hierarchical)
#---------------------------------------------------------------
puts "--- net queries ---"

set all_nets [get_nets *]
puts "flat nets: [llength $all_nets]"

set hier_nets [get_nets -hierarchical *]
puts "hierarchical nets: [llength $hier_nets]"

# Pattern matching
set w_nets [get_nets w*]
puts "w* nets: [llength $w_nets]"

set hier_w_nets [get_nets -hierarchical w*]
puts "w* hier nets: [llength $hier_w_nets]"

# Specific net properties
foreach net_name {w1 w2 w3 w4 w5} {
  set net [get_nets $net_name]
  set fn [get_full_name $net]
  puts "net $net_name: name=$fn"
}

#---------------------------------------------------------------
# Fanin/fanout traversal through hierarchy
# Exercises: visitDrvrLoadsThruHierPin, HpinDrvrLoad
#---------------------------------------------------------------
puts "--- fanin/fanout traversal ---"

set fi1 [get_fanin -to [get_ports out1] -flat]
puts "fanin to out1 flat: [llength $fi1]"

set fi2 [get_fanin -to [get_ports out1] -only_cells]
puts "fanin to out1 cells: [llength $fi2]"

set fi3 [get_fanin -to [get_ports out1] -startpoints_only]
puts "fanin to out1 startpoints: [llength $fi3]"

set fo1 [get_fanout -from [get_ports in1] -flat]
puts "fanout from in1 flat: [llength $fo1]"

set fo2 [get_fanout -from [get_ports in1] -only_cells]
puts "fanout from in1 cells: [llength $fo2]"

set fo3 [get_fanout -from [get_ports in1] -endpoints_only]
puts "fanout from in1 endpoints: [llength $fo3]"

# Through different paths
set fi_out2 [get_fanin -to [get_ports out2] -flat]
puts "fanin to out2 flat: [llength $fi_out2]"

set fo_in2 [get_fanout -from [get_ports in2] -flat]
puts "fanout from in2 flat: [llength $fo_in2]"

set fo_in3 [get_fanout -from [get_ports in3] -flat]
puts "fanout from in3 flat: [llength $fo_in3]"

# Different trace_arcs modes
set fi_timing [get_fanin -to [get_ports out1] -flat -trace_arcs timing]
puts "fanin timing: [llength $fi_timing]"

set fi_all [get_fanin -to [get_ports out1] -flat -trace_arcs all]
puts "fanin all: [llength $fi_all]"

set fo_timing [get_fanout -from [get_ports in1] -flat -trace_arcs timing]
puts "fanout timing: [llength $fo_timing]"

set fo_all [get_fanout -from [get_ports in1] -flat -trace_arcs all]
puts "fanout all: [llength $fo_all]"

#---------------------------------------------------------------
# Detailed timing through hierarchy
# Exercises: arc evaluation, path delay calculation
#---------------------------------------------------------------
puts "--- timing reports ---"
report_checks -from [get_ports in1] -to [get_ports out1]
report_checks -from [get_ports in2] -to [get_ports out1]
report_checks -from [get_ports in3] -to [get_ports out2]
report_checks -path_delay min
report_checks -path_delay max

# Detailed reports with various fields
report_checks -fields {slew cap input_pins nets fanout}
report_checks -format full_clock
report_checks -format full_clock_expanded

# Reports with endpoint/group counts
report_checks -endpoint_count 3
report_checks -group_count 2

#---------------------------------------------------------------
# Network modification in hierarchical context
#---------------------------------------------------------------
puts "--- network modify in hierarchy ---"
set new_buf [make_instance hier_test_buf NangateOpenCellLibrary/BUF_X1]
set new_inv [make_instance hier_test_inv NangateOpenCellLibrary/INV_X1]
set new_net1 [make_net hier_test_net1]
set new_net2 [make_net hier_test_net2]

# Connect new instances
connect_pin hier_test_net1 hier_test_buf/A
connect_pin hier_test_net2 hier_test_buf/Z
connect_pin hier_test_net2 hier_test_inv/A

# Disconnect
disconnect_pin hier_test_net1 hier_test_buf/A
disconnect_pin hier_test_net2 hier_test_buf/Z
disconnect_pin hier_test_net2 hier_test_inv/A

# Clean up
delete_instance hier_test_buf
delete_instance hier_test_inv
delete_net hier_test_net1
delete_net hier_test_net2

#---------------------------------------------------------------
# Register queries
#---------------------------------------------------------------
puts "--- register queries ---"
set regs [all_registers]
puts "all_registers: [llength $regs]"

set reg_data [all_registers -data_pins]
puts "data_pins: [llength $reg_data]"

set reg_clk [all_registers -clock_pins]
puts "clock_pins: [llength $reg_clk]"

set reg_out [all_registers -output_pins]
puts "output_pins: [llength $reg_out]"
