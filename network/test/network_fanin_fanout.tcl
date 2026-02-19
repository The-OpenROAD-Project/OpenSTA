# Test network fanin/fanout traversal, pin direction queries, connected pins,
# and hierarchical instance/net/pin queries.
# Targets:
#   Network.cc: visitConnectedPins (pin/net/recursive), connectedPinIterator,
#     isDriver, isLoad, isRegClkPin, isCheckClk, isLatchData,
#     isInside (instance/pin/net variants), isHierarchical (instance/pin),
#     isTopLevelPort, isTopInstance, isLeaf,
#     highestNetAbove, highestConnectedNet, connectedNets,
#     hierarchyLevel, findNetsMatching, findNetsHierMatching,
#     findPinsMatching, findPinsHierMatching, findInstPinsHierMatching,
#     findInstancesHierMatching, findChildrenMatching,
#     pathName (Instance/Pin/Net/Term), pathNameCmp, pathNameLess,
#     name (Term), portName (Term)
#   ConcreteNetwork.cc: connectedPinIterator, netIterator,
#     findPort, findPin with various patterns,
#     childIterator, pinIterator (instance/net)

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Load hierarchical design for thorough coverage
#---------------------------------------------------------------
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog network_hier_test.v
link_design network_hier_test

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {in1 in2 in3}]
set_output_delay -clock clk 0 [get_ports {out1 out2}]
set_input_transition 0.1 [all_inputs]

# Force timing graph construction
report_checks

#---------------------------------------------------------------
# Test pin direction queries: isDriver, isLoad, isRegClkPin
#---------------------------------------------------------------
puts "--- pin direction queries ---"

# Input port is a driver (top-level input drives into design)
set in1_port [get_ports in1]
puts "in1 is input port"

# Output port is a load (top-level output is load on internal net)
set out1_port [get_ports out1]
puts "out1 is output port"

# Reg clock pin
set reg1_ck [get_pins reg1/CK]
puts "reg1/CK direction: [get_property $reg1_ck direction]"

# Reg data pin
set reg1_d [get_pins reg1/D]
puts "reg1/D direction: [get_property $reg1_d direction]"

# Reg output pin
set reg1_q [get_pins reg1/Q]
puts "reg1/Q direction: [get_property $reg1_q direction]"

# Check all pin directions across instances
foreach pin_path {buf_in/A buf_in/Z inv1/A inv1/ZN buf_out1/A buf_out1/Z buf_out2/A buf_out2/Z} {
  set pin [get_pins $pin_path]
  set dir [get_property $pin direction]
  puts "pin $pin_path: dir=$dir"
}

#---------------------------------------------------------------
# Test hierarchical instance/pin queries
#---------------------------------------------------------------
puts "--- hierarchical queries ---"

# Get all cells hierarchical
set hier_cells [get_cells -hierarchical *]
puts "hierarchical cells: [llength $hier_cells]"

# Get all pins hierarchical
set hier_pins [get_pins -hierarchical *]
puts "hierarchical pins: [llength $hier_pins]"

# Get all nets hierarchical
set hier_nets [get_nets -hierarchical *]
puts "hierarchical nets: [llength $hier_nets]"

# Hierarchical instance with pattern
set sub_cells [get_cells -hierarchical sub*]
puts "sub* hierarchical cells: [llength $sub_cells]"

# Find instances matching pattern
set buf_cells [get_cells -hierarchical buf*]
puts "buf* hierarchical cells: [llength $buf_cells]"

#---------------------------------------------------------------
# Test get_fanin and get_fanout with various options
# This exercises visitFaninPins/visitFanoutPins in Network.cc
#---------------------------------------------------------------
puts "--- fanin/fanout variants ---"

# Flat fanin to output port
set fi_flat [get_fanin -to [get_ports out1] -flat]
puts "fanin flat to out1: [llength $fi_flat]"

# Flat fanin only cells
set fi_cells [get_fanin -to [get_ports out1] -only_cells]
puts "fanin cells to out1: [llength $fi_cells]"

# Flat fanin startpoints only
set fi_start [get_fanin -to [get_ports out1] -startpoints_only]
puts "fanin startpoints to out1: [llength $fi_start]"

# Flat fanout from input port
set fo_flat [get_fanout -from [get_ports in1] -flat]
puts "fanout flat from in1: [llength $fo_flat]"

# Flat fanout only cells
set fo_cells [get_fanout -from [get_ports in1] -only_cells]
puts "fanout cells from in1: [llength $fo_cells]"

# Flat fanout endpoints only
set fo_end [get_fanout -from [get_ports in1] -endpoints_only]
puts "fanout endpoints from in1: [llength $fo_end]"

# Fanin with levels
set fi_lev1 [get_fanin -to [get_ports out1] -flat -levels 1]
puts "fanin levels=1 to out1: [llength $fi_lev1]"

set fi_lev2 [get_fanin -to [get_ports out1] -flat -levels 2]
puts "fanin levels=2 to out1: [llength $fi_lev2]"

set fi_lev3 [get_fanin -to [get_ports out1] -flat -levels 3]
puts "fanin levels=3 to out1: [llength $fi_lev3]"

# Fanin with pin_levels
set fi_plev1 [get_fanin -to [get_ports out1] -flat -pin_levels 1]
puts "fanin pin_levels=1 to out1: [llength $fi_plev1]"

set fi_plev2 [get_fanin -to [get_ports out1] -flat -pin_levels 2]
puts "fanin pin_levels=2 to out1: [llength $fi_plev2]"

# Fanout with levels
set fo_lev1 [get_fanout -from [get_ports in1] -flat -levels 1]
puts "fanout levels=1 from in1: [llength $fo_lev1]"

set fo_lev2 [get_fanout -from [get_ports in1] -flat -levels 2]
puts "fanout levels=2 from in1: [llength $fo_lev2]"

# Fanout with pin_levels
set fo_plev1 [get_fanout -from [get_ports in1] -flat -pin_levels 1]
puts "fanout pin_levels=1 from in1: [llength $fo_plev1]"

set fo_plev2 [get_fanout -from [get_ports in1] -flat -pin_levels 2]
puts "fanout pin_levels=2 from in1: [llength $fo_plev2]"

# Fanin/fanout with trace_arcs options
set fi_timing [get_fanin -to [get_ports out1] -flat -trace_arcs timing]
puts "fanin trace_arcs timing to out1: [llength $fi_timing]"

set fi_enabled [get_fanin -to [get_ports out1] -flat -trace_arcs enabled]
puts "fanin trace_arcs enabled to out1: [llength $fi_enabled]"

set fi_all [get_fanin -to [get_ports out1] -flat -trace_arcs all]
puts "fanin trace_arcs all to out1: [llength $fi_all]"

set fo_all [get_fanout -from [get_ports in1] -flat -trace_arcs all]
puts "fanout trace_arcs all from in1: [llength $fo_all]"

# Fanin/fanout from second output
set fi_out2 [get_fanin -to [get_ports out2] -flat]
puts "fanin flat to out2: [llength $fi_out2]"

set fi_out2_cells [get_fanin -to [get_ports out2] -only_cells]
puts "fanin cells to out2: [llength $fi_out2_cells]"

# Fanout from other inputs
set fo_in2 [get_fanout -from [get_ports in2] -flat]
puts "fanout flat from in2: [llength $fo_in2]"

set fo_in3 [get_fanout -from [get_ports in3] -flat]
puts "fanout flat from in3: [llength $fo_in3]"

#---------------------------------------------------------------
# Test report_net for all internal nets
#---------------------------------------------------------------
puts "--- report_net all ---"
foreach net_name {w1 w2 w3 w4 w5} {
  report_net $net_name
  puts "report_net $net_name: done"
}

#---------------------------------------------------------------
# Test report_instance for all instances including hierarchical
#---------------------------------------------------------------
puts "--- report_instance all ---"
foreach inst_name {buf_in sub1 sub2 inv1 reg1 buf_out1 buf_out2} {
  report_instance $inst_name
}

#---------------------------------------------------------------
# Test all_registers variants (exercises isRegClkPin paths)
#---------------------------------------------------------------
puts "--- all_registers ---"
set regs [all_registers]
puts "all_registers: [llength $regs]"

set reg_data [all_registers -data_pins]
puts "register data_pins: [llength $reg_data]"

set reg_clk [all_registers -clock_pins]
puts "register clock_pins: [llength $reg_clk]"

set reg_out [all_registers -output_pins]
puts "register output_pins: [llength $reg_out]"

#---------------------------------------------------------------
# Test get_ports with direction filter
# Exercises hasDirection paths
#---------------------------------------------------------------
puts "--- port direction filters ---"
set in_ports [get_ports -filter "direction == input"]
puts "input ports: [llength $in_ports]"

set out_ports [get_ports -filter "direction == output"]
puts "output ports: [llength $out_ports]"

#---------------------------------------------------------------
# Test get_cells with ref_name filter
#---------------------------------------------------------------
puts "--- cell ref_name filters ---"
set buf_x1_cells [get_cells -filter "ref_name == BUF_X1" *]
puts "BUF_X1: [llength $buf_x1_cells]"

set dff_cells [get_cells -filter "ref_name == DFF_X1" *]
puts "DFF_X1: [llength $dff_cells]"

set inv_cells [get_cells -filter "ref_name == INV_X1" *]
puts "INV_X1: [llength $inv_cells]"

#---------------------------------------------------------------
# Instance count and pin count queries
#---------------------------------------------------------------
puts "--- instance/pin/net count queries ---"
puts "top level cells: [llength [get_cells *]]"
puts "top level nets: [llength [get_nets *]]"
puts "top level pins: [llength [get_pins */*]]"
