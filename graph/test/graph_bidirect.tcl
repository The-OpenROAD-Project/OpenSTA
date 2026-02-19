# Test graph construction with bidirectional pins, reconvergent paths,
# and various edge/vertex operations.
# Targets: Graph.cc uncovered paths:
#   makePinVertices for bidirect pins
#   pinVertices for bidirect direction
#   pinDrvrVertex for bidirect
#   makePortInstanceEdges: bidirect from_bidirect_drvr_vertex path
#   makeWireEdgesFromPin with multiple drivers
#   hasFaninOne
#   gateEdgeArc
#   deleteVertex via delete operations
#   isIsolatedNet
#   vertex/edge iterators

source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog graph_bidirect.v
link_design graph_bidirect

#---------------------------------------------------------------
# Test 1: Graph construction and basic timing
#---------------------------------------------------------------
puts "--- Test 1: graph with reconvergent paths ---"
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {d1 d2 d3 d4}]
set_output_delay -clock clk 0 [get_ports {q1 q2 q3 q4}]
set_input_transition 0.1 [get_ports {d1 d2 d3 d4 clk}]

report_checks

report_checks -path_delay min

#---------------------------------------------------------------
# Test 2: Multiple path queries (exercises graph traversal)
#---------------------------------------------------------------
puts "--- Test 2: path queries ---"
report_checks -from [get_ports d1] -to [get_ports q1]

report_checks -from [get_ports d1] -to [get_ports q2]

report_checks -from [get_ports d2] -to [get_ports q1]

report_checks -from [get_ports d3] -to [get_ports q3]

report_checks -from [get_ports d4] -to [get_ports q3]

report_checks -from [get_ports d1] -to [get_ports q4]

report_checks -from [get_ports d3] -to [get_ports q4]

#---------------------------------------------------------------
# Test 3: Fields that exercise graph delay/slew queries
#---------------------------------------------------------------
puts "--- Test 3: report with fields ---"
report_checks -fields {slew cap input_pins nets fanout}

report_checks -format full_clock

report_checks -path_delay min -fields {slew cap}

#---------------------------------------------------------------
# Test 4: Fanin/fanout queries through reconvergent paths
#---------------------------------------------------------------
puts "--- Test 4: fanin/fanout ---"
set fi [get_fanin -to [get_ports q2] -flat]
puts "fanin to q2: [llength $fi]"

set fo [get_fanout -from [get_ports d1] -flat]
puts "fanout from d1: [llength $fo]"

set fi_cells [get_fanin -to [get_ports q2] -only_cells]
puts "fanin cells to q2: [llength $fi_cells]"

set fo_cells [get_fanout -from [get_ports d1] -only_cells]
puts "fanout cells from d1: [llength $fo_cells]"

set fi_q3 [get_fanin -to [get_ports q3] -flat]
puts "fanin to q3: [llength $fi_q3]"

set fo_d3 [get_fanout -from [get_ports d3] -flat]
puts "fanout from d3: [llength $fo_d3]"

#---------------------------------------------------------------
# Test 5: report_dcalc exercises graph edge arc queries
#---------------------------------------------------------------
puts "--- Test 5: report_dcalc ---"
report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max
puts "dcalc buf1: done"

report_dcalc -from [get_pins and1/A1] -to [get_pins and1/ZN] -max
puts "dcalc and1 A1: done"

report_dcalc -from [get_pins and1/A2] -to [get_pins and1/ZN] -max
puts "dcalc and1 A2: done"

report_dcalc -from [get_pins or1/A1] -to [get_pins or1/ZN] -max
puts "dcalc or1 A1: done"

report_dcalc -from [get_pins nand1/A1] -to [get_pins nand1/ZN] -max
puts "dcalc nand1: done"

report_dcalc -from [get_pins nor1/A1] -to [get_pins nor1/ZN] -max
puts "dcalc nor1: done"

report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/Q] -max
puts "dcalc reg1: done"

#---------------------------------------------------------------
# Test 6: Graph vertex/edge counts
#---------------------------------------------------------------
puts "--- Test 6: network queries ---"
set all_pins [get_pins */*]
puts "total pins: [llength $all_pins]"

set all_nets [get_nets *]
puts "total nets: [llength $all_nets]"

foreach net_name {n1 n2 n3 n4 n5 n6 n7 n8 n9 n10} {
  report_net $net_name
}

foreach inst_name {buf1 buf2 inv1 inv2 and1 or1 nand1 nor1 and2 or2 reg1 reg2 reg3 reg4} {
  report_instance $inst_name
}

#---------------------------------------------------------------
# Test 7: Add and remove instances (exercises deleteVertex, graph modify)
#---------------------------------------------------------------
puts "--- Test 7: modify graph ---"
set new_net [make_net test_net]
set new_inst [make_instance test_buf BUF_X1]
connect_pin test_net test_buf/A

report_checks

disconnect_pin test_net test_buf/A
delete_instance test_buf
delete_net test_net

report_checks
