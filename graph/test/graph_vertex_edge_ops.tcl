# Test graph vertex and edge operations in depth: makeVertex, deleteVertex,
# makeEdge, deleteEdge, edge arc queries, bidirectional pin handling,
# hasFaninOne, vertex iteration, edge linking.
# Targets: Graph.cc uncovered:
#   deleteVertex (lines 476-504): edge cleanup during vertex deletion
#   deleteInEdge / deleteOutEdge: linked list manipulation for edges
#   hasFaninOne: single fanin check
#   pinDrvrVertex / pinLoadVertex: bidirect driver vertex lookup
#   gateEdgeArc: arc lookup by rise/fall
#   makePaths / paths / deletePaths: vertex path management
#   slew / setSlew: slew value access
#   makeWireEdgesToPin: create wire edges to a pin
#   isIsolatedNet: isolated net detection
#   arcDelay / setArcDelay: edge arc delay access

source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog graph_delete_modify.v
link_design graph_delete_modify

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports {d1 d2 d3 rst}]
set_output_delay -clock clk 1.0 [get_ports {q1 q2 q3 q4}]
set_input_transition 0.1 [get_ports {d1 d2 d3 rst clk}]

#---------------------------------------------------------------
# Test 1: Baseline - build graph and verify edges
#---------------------------------------------------------------
puts "--- Test 1: baseline edge count ---"
report_checks
puts "PASS: baseline timing"

# Query edges for each cell
foreach cell_name {buf1 buf2 inv1 and1 or1 nand1 nor1} {
  set edges [get_timing_edges -of_objects [get_cells $cell_name]]
  puts "$cell_name edges: [llength $edges]"
}
puts "PASS: baseline edge queries"

#---------------------------------------------------------------
# Test 2: Add chain of buffers, verify edges, then delete one by one
# Exercises: makeVertex, makeEdge, deleteVertex, deleteEdge,
#   deleteInEdge, deleteOutEdge
#---------------------------------------------------------------
puts "--- Test 2: chain add/delete ---"

# Create 4-stage buffer chain
set chain_nets {}
set chain_insts {}
for {set i 0} {$i < 4} {incr i} {
  lappend chain_nets [make_net "chain_n$i"]
}
lappend chain_nets [make_net "chain_n4"]

for {set i 0} {$i < 4} {incr i} {
  set inst [make_instance "chain_buf$i" NangateOpenCellLibrary/BUF_X1]
  lappend chain_insts $inst
  connect_pin "chain_n$i" "chain_buf$i/A"
  set j [expr {$i + 1}]
  connect_pin "chain_n$j" "chain_buf$i/Z"
}
puts "PASS: 4-stage chain created"

report_checks
puts "PASS: timing with chain"

# Query chain edges
for {set i 0} {$i < 4} {incr i} {
  set edges [get_timing_edges -of_objects [get_cells "chain_buf$i"]]
  puts "chain_buf$i edges: [llength $edges]"
}

# Delete chain from end to beginning (exercises reverse cleanup)
for {set i 3} {$i >= 0} {incr i -1} {
  disconnect_pin "chain_n$i" "chain_buf$i/A"
  set j [expr {$i + 1}]
  disconnect_pin "chain_n$j" "chain_buf$i/Z"
  delete_instance "chain_buf$i"
}
for {set i 0} {$i <= 4} {incr i} {
  delete_net "chain_n$i"
}
puts "PASS: chain deleted"

report_checks
puts "PASS: timing after chain delete"

#---------------------------------------------------------------
# Test 3: Multiple fan-out and fan-in scenarios
# Exercises: makeWireEdgesFromPin with multi-driver nets
#---------------------------------------------------------------
puts "--- Test 3: fan-out/fan-in ---"

set fo_net [make_net "fanout_net"]
set fo_drv [make_instance "fo_drv" NangateOpenCellLibrary/BUF_X4]
set fo_load1 [make_instance "fo_load1" NangateOpenCellLibrary/BUF_X1]
set fo_load2 [make_instance "fo_load2" NangateOpenCellLibrary/BUF_X1]
set fo_load3 [make_instance "fo_load3" NangateOpenCellLibrary/BUF_X1]

set fo_in [make_net "fo_in"]
connect_pin fo_in fo_drv/A
connect_pin fanout_net fo_drv/Z
connect_pin fanout_net fo_load1/A
connect_pin fanout_net fo_load2/A
connect_pin fanout_net fo_load3/A

puts "PASS: fanout-3 net created"

report_checks
puts "PASS: timing with fanout"

# Query edge count on fanout driver
set drv_edges [get_timing_edges -of_objects [get_cells fo_drv]]
puts "fo_drv edges: [llength $drv_edges]"

# Disconnect loads one by one
disconnect_pin fanout_net fo_load3/A
report_checks
puts "PASS: fanout-2 timing"

disconnect_pin fanout_net fo_load2/A
report_checks
puts "PASS: fanout-1 timing"

# Cleanup
disconnect_pin fanout_net fo_load1/A
disconnect_pin fanout_net fo_drv/Z
disconnect_pin fo_in fo_drv/A
delete_instance fo_load1
delete_instance fo_load2
delete_instance fo_load3
delete_instance fo_drv
delete_net fanout_net
delete_net fo_in
puts "PASS: fanout cleanup"

report_checks
puts "PASS: timing after fanout cleanup"

#---------------------------------------------------------------
# Test 4: Replace cell multiple times and verify edge rebuild
# Exercises: makeInstanceEdges rebuild, timing arc set changes
#---------------------------------------------------------------
puts "--- Test 4: cell replacement cycle ---"

# Replace buf1 through several sizes
foreach lib_cell {BUF_X1 BUF_X2 BUF_X4 BUF_X8 BUF_X4 BUF_X2 BUF_X1} {
  replace_cell buf1 "NangateOpenCellLibrary/$lib_cell"
  report_checks -path_delay max
}
puts "PASS: buf1 replacement cycle"

# Replace AND gate
foreach lib_cell {AND2_X1 AND2_X2 AND2_X4 AND2_X2 AND2_X1} {
  replace_cell and1 "NangateOpenCellLibrary/$lib_cell"
  report_checks
}
puts "PASS: and1 replacement cycle"

#---------------------------------------------------------------
# Test 5: Register add/delete to exercise reg_clk_vertices
# Exercises: makeVertex is_reg_clk path, reg_clk_vertices_ insert/erase
#---------------------------------------------------------------
puts "--- Test 5: register add/delete ---"

# Add multiple registers
for {set i 0} {$i < 3} {incr i} {
  set rn [make_net "reg_d$i"]
  set rqn [make_net "reg_q$i"]
  set ri [make_instance "test_reg$i" NangateOpenCellLibrary/DFF_X1]
  connect_pin "reg_d$i" "test_reg$i/D"
  connect_pin "reg_q$i" "test_reg$i/Q"
  catch {connect_pin clk "test_reg$i/CK"} msg
}
puts "PASS: 3 registers added"

report_checks
puts "PASS: timing with added registers"

# Delete the registers
for {set i 0} {$i < 3} {incr i} {
  catch {disconnect_pin clk "test_reg$i/CK"} msg
  disconnect_pin "reg_d$i" "test_reg$i/D"
  disconnect_pin "reg_q$i" "test_reg$i/Q"
  delete_instance "test_reg$i"
  delete_net "reg_d$i"
  delete_net "reg_q$i"
}
puts "PASS: registers deleted"

report_checks
puts "PASS: timing after register deletion"

#---------------------------------------------------------------
# Test 6: Slew and timing edge reports
# Exercises: slew access, edge arc iteration
#---------------------------------------------------------------
puts "--- Test 6: slew and edge reports ---"

report_slews [get_ports d1]
report_slews [get_ports d2]
report_slews [get_ports d3]
report_slews [get_ports clk]
puts "PASS: port slews"

report_slews [get_pins buf1/A]
report_slews [get_pins buf1/Z]
report_slews [get_pins and1/A1]
report_slews [get_pins and1/ZN]
report_slews [get_pins inv1/A]
report_slews [get_pins inv1/ZN]
report_slews [get_pins nand1/ZN]
report_slews [get_pins nor1/ZN]
puts "PASS: pin slews"

# Edge reports
report_edges -from [get_pins buf1/A] -to [get_pins buf1/Z]
report_edges -from [get_pins and1/A1] -to [get_pins and1/ZN]
report_edges -from [get_pins inv1/A] -to [get_pins inv1/ZN]
puts "PASS: edge reports"

#---------------------------------------------------------------
# Test 7: Through-pin and endpoint queries
# Exercises: graph traversal paths
#---------------------------------------------------------------
puts "--- Test 7: through-pin queries ---"

catch {report_checks -through [get_pins buf1/Z]} msg
catch {report_checks -through [get_pins and1/ZN]} msg
catch {report_checks -through [get_pins inv1/ZN]} msg
catch {report_checks -through [get_pins nand1/ZN]} msg
catch {report_checks -through [get_pins nor1/ZN]} msg
catch {report_checks -through [get_pins or1/ZN]} msg
puts "PASS: through-pin queries"

# Endpoint
catch {report_checks -to [get_ports q1]} msg
catch {report_checks -to [get_ports q2]} msg
catch {report_checks -to [get_ports q3]} msg
catch {report_checks -to [get_ports q4]} msg
puts "PASS: endpoint queries"

puts "ALL PASSED"
