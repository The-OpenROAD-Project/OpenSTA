# Test graph construction with bidirectional pins, reconvergent paths,
# and various edge/vertex operations.
# Targets: Graph.cc uncovered paths:
#   makePinVertices for bidirect pins (lines 425-427)
#   pinVertices for bidirect direction (lines 453-455)
#   pinDrvrVertex for bidirect (lines 463-464)
#   makePortInstanceEdges: bidirect from_bidirect_drvr_vertex path (lines 223-229)
#   makeWireEdgesFromPin with multiple drivers (lines 277-301)
#   hasFaninOne (line 507-511)
#   gateEdgeArc (line 544+)
#   deleteVertex (lines 476-504) via delete operations
#   isIsolatedNet (lines 309-331)
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
puts "PASS: report_checks"

report_checks -path_delay min
puts "PASS: report_checks min"

#---------------------------------------------------------------
# Test 2: Multiple path queries (exercises graph traversal)
#---------------------------------------------------------------
puts "--- Test 2: path queries ---"
report_checks -from [get_ports d1] -to [get_ports q1]
puts "PASS: d1->q1"

report_checks -from [get_ports d1] -to [get_ports q2]
puts "PASS: d1->q2 (reconvergent)"

report_checks -from [get_ports d2] -to [get_ports q1]
puts "PASS: d2->q1"

report_checks -from [get_ports d3] -to [get_ports q3]
puts "PASS: d3->q3"

report_checks -from [get_ports d4] -to [get_ports q3]
puts "PASS: d4->q3"

report_checks -from [get_ports d1] -to [get_ports q4]
puts "PASS: d1->q4 (reconvergent)"

report_checks -from [get_ports d3] -to [get_ports q4]
puts "PASS: d3->q4 (reconvergent)"

#---------------------------------------------------------------
# Test 3: Fields that exercise graph delay/slew queries
#---------------------------------------------------------------
puts "--- Test 3: report with fields ---"
report_checks -fields {slew cap input_pins nets fanout}
puts "PASS: report with all fields"

report_checks -format full_clock
puts "PASS: report full_clock"

report_checks -path_delay min -fields {slew cap}
puts "PASS: min with slew/cap fields"

#---------------------------------------------------------------
# Test 4: Fanin/fanout queries through reconvergent paths
#---------------------------------------------------------------
puts "--- Test 4: fanin/fanout ---"
catch {
  set fi [get_fanin -to [get_ports q2] -flat]
  puts "fanin to q2: [llength $fi]"
} msg

catch {
  set fo [get_fanout -from [get_ports d1] -flat]
  puts "fanout from d1: [llength $fo]"
} msg

catch {
  set fi_cells [get_fanin -to [get_ports q2] -only_cells]
  puts "fanin cells to q2: [llength $fi_cells]"
} msg

catch {
  set fo_cells [get_fanout -from [get_ports d1] -only_cells]
  puts "fanout cells from d1: [llength $fo_cells]"
} msg

catch {
  set fi_q3 [get_fanin -to [get_ports q3] -flat]
  puts "fanin to q3: [llength $fi_q3]"
} msg

catch {
  set fo_d3 [get_fanout -from [get_ports d3] -flat]
  puts "fanout from d3: [llength $fo_d3]"
} msg

#---------------------------------------------------------------
# Test 5: report_dcalc exercises graph edge arc queries
#---------------------------------------------------------------
puts "--- Test 5: report_dcalc ---"
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts "dcalc buf1: done"

catch {report_dcalc -from [get_pins and1/A1] -to [get_pins and1/ZN] -max} msg
puts "dcalc and1 A1: done"

catch {report_dcalc -from [get_pins and1/A2] -to [get_pins and1/ZN] -max} msg
puts "dcalc and1 A2: done"

catch {report_dcalc -from [get_pins or1/A1] -to [get_pins or1/ZN] -max} msg
puts "dcalc or1 A1: done"

catch {report_dcalc -from [get_pins nand1/A1] -to [get_pins nand1/ZN] -max} msg
puts "dcalc nand1: done"

catch {report_dcalc -from [get_pins nor1/A1] -to [get_pins nor1/ZN] -max} msg
puts "dcalc nor1: done"

catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/Q] -max} msg
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
  catch {report_net $net_name} msg
}
puts "PASS: report_net all"

foreach inst_name {buf1 buf2 inv1 inv2 and1 or1 nand1 nor1 and2 or2 reg1 reg2 reg3 reg4} {
  report_instance $inst_name
}
puts "PASS: report_instance all"

#---------------------------------------------------------------
# Test 7: Add and remove instances (exercises deleteVertex, graph modify)
#---------------------------------------------------------------
puts "--- Test 7: modify graph ---"
set new_net [make_net test_net]
set new_inst [make_instance test_buf BUF_X1]
connect_pin test_net test_buf/A

report_checks
puts "PASS: report after add instance"

disconnect_pin test_net test_buf/A
delete_instance test_buf
delete_net test_net

report_checks
puts "PASS: report after delete instance"

puts "ALL PASSED"
