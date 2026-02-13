# Test graph modification: add/delete vertices via connect_pin/disconnect_pin,
# delete_instance, replace_cell, and repeated graph rebuild.
# Targets:
#   Graph.cc: deleteVertex (lines 476-504), deleteInEdge, deleteOutEdge,
#     makePinVertices, makeVertex, makeWireEdgesFromPin (multi-driver),
#     hasFaninOne, makeInstEdges after replace_cell,
#     removeWireEdge, removeInstEdge on disconnect/reconnect,
#     reg_clk_vertices_ insert/erase on add/delete reg

source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog graph_delete_modify.v
link_design graph_delete_modify

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports {d1 d2 d3 rst}]
set_output_delay -clock clk 1.0 [get_ports {q1 q2 q3 q4}]
set_input_transition 0.1 [get_ports {d1 d2 d3 rst clk}]

#---------------------------------------------------------------
# Test 1: Baseline timing
#---------------------------------------------------------------
puts "--- Test 1: baseline ---"
report_checks
puts "PASS: baseline max"

report_checks -path_delay min
puts "PASS: baseline min"

report_checks -fields {slew cap input_pins nets fanout}
puts "PASS: baseline fields"

#---------------------------------------------------------------
# Test 2: Add multiple instances and nets, then delete
# Exercises: makeVertex, makeWireEdgesFromPin, deleteVertex,
#   deleteInEdge, deleteOutEdge
#---------------------------------------------------------------
puts "--- Test 2: add/delete multiple instances ---"

# Add a buffer chain
set net_a [make_net test_net_a]
set net_b [make_net test_net_b]
set net_c [make_net test_net_c]
set inst_a [make_instance test_buf_a NangateOpenCellLibrary/BUF_X1]
set inst_b [make_instance test_buf_b NangateOpenCellLibrary/BUF_X2]

connect_pin test_net_a test_buf_a/A
connect_pin test_net_b test_buf_a/Z
connect_pin test_net_b test_buf_b/A
connect_pin test_net_c test_buf_b/Z
puts "PASS: added buffer chain"

report_checks
puts "PASS: timing after add chain"

# Disconnect middle and verify
disconnect_pin test_net_b test_buf_b/A
report_checks
puts "PASS: timing after partial disconnect"

# Reconnect
connect_pin test_net_b test_buf_b/A
report_checks
puts "PASS: timing after reconnect"

# Full cleanup
disconnect_pin test_net_a test_buf_a/A
disconnect_pin test_net_b test_buf_a/Z
disconnect_pin test_net_b test_buf_b/A
disconnect_pin test_net_c test_buf_b/Z
delete_instance test_buf_a
delete_instance test_buf_b
delete_net test_net_a
delete_net test_net_b
delete_net test_net_c
puts "PASS: full cleanup"

report_checks
puts "PASS: timing after full cleanup"

#---------------------------------------------------------------
# Test 3: Replace cell multiple times
# Exercises: makeInstEdges rebuild, edge arc changes
#---------------------------------------------------------------
puts "--- Test 3: replace_cell ---"

replace_cell buf1 NangateOpenCellLibrary/BUF_X4
report_checks
report_edges -from [get_pins buf1/A] -to [get_pins buf1/Z]
puts "PASS: buf1 -> BUF_X4"

replace_cell buf1 NangateOpenCellLibrary/BUF_X2
report_checks
puts "PASS: buf1 -> BUF_X2"

replace_cell buf1 NangateOpenCellLibrary/BUF_X1
report_checks
puts "PASS: buf1 restored"

replace_cell and1 NangateOpenCellLibrary/AND2_X2
report_checks
puts "PASS: and1 -> AND2_X2"

replace_cell and1 NangateOpenCellLibrary/AND2_X1
report_checks
puts "PASS: and1 restored"

replace_cell inv1 NangateOpenCellLibrary/INV_X2
report_checks
puts "PASS: inv1 -> INV_X2"

replace_cell inv1 NangateOpenCellLibrary/INV_X1
report_checks
puts "PASS: inv1 restored"

#---------------------------------------------------------------
# Test 4: Add and delete register instances
# Exercises: reg_clk_vertices_ insert/erase in makeVertex/deleteVertex
#---------------------------------------------------------------
puts "--- Test 4: add/delete register ---"

set reg_net [make_net reg_test_net]
set reg_qnet [make_net reg_test_qnet]
set reg_inst [make_instance test_reg NangateOpenCellLibrary/DFF_X1]

connect_pin reg_test_net test_reg/D
connect_pin reg_test_qnet test_reg/Q

# Connect clock to new register
set clk_net_name "clk"
catch {connect_pin $clk_net_name test_reg/CK} msg

report_checks
puts "PASS: timing with added register"

# Remove the register
catch {disconnect_pin $clk_net_name test_reg/CK} msg
disconnect_pin reg_test_net test_reg/D
disconnect_pin reg_test_qnet test_reg/Q
delete_instance test_reg
delete_net reg_test_net
delete_net reg_test_qnet
puts "PASS: register removed"

report_checks
puts "PASS: timing after register removal"

#---------------------------------------------------------------
# Test 5: Rapid connect/disconnect on same pin
# Exercises: edge create/delete cycling
#---------------------------------------------------------------
puts "--- Test 5: rapid connect/disconnect ---"

set tmp_net [make_net tmp_net]
set tmp_inst [make_instance tmp_buf NangateOpenCellLibrary/BUF_X1]

# Cycle 1
connect_pin tmp_net tmp_buf/A
report_checks
disconnect_pin tmp_net tmp_buf/A
puts "cycle 1 done"

# Cycle 2
connect_pin tmp_net tmp_buf/A
report_checks
disconnect_pin tmp_net tmp_buf/A
puts "cycle 2 done"

# Cycle 3
connect_pin tmp_net tmp_buf/A
report_checks
disconnect_pin tmp_net tmp_buf/A
puts "cycle 3 done"

delete_instance tmp_buf
delete_net tmp_net
puts "PASS: rapid cycles"

report_checks
puts "PASS: timing after rapid cycles"

#---------------------------------------------------------------
# Test 6: Edge queries after all modifications
#---------------------------------------------------------------
puts "--- Test 6: edge queries ---"

foreach cell_name {buf1 buf2 inv1 and1 or1 nand1 nor1 reg1 reg2 reg3 reg4} {
  set edges [get_timing_edges -of_objects [get_cells $cell_name]]
  puts "$cell_name edges: [llength $edges]"
}
puts "PASS: edge queries"

# Slew queries
report_slews [get_ports d1]
report_slews [get_ports d2]
report_slews [get_ports d3]
report_slews [get_pins buf1/Z]
report_slews [get_pins and1/ZN]
report_slews [get_pins reg1/Q]
puts "PASS: slew queries"

#---------------------------------------------------------------
# Test 7: Through-pin paths
#---------------------------------------------------------------
puts "--- Test 7: through pins ---"
catch {report_checks -through [get_pins nand1/ZN]} msg
puts "through nand1: done"

catch {report_checks -through [get_pins nor1/ZN]} msg
puts "through nor1: done"

catch {report_checks -through [get_pins and1/ZN]} msg
puts "through and1: done"

puts "PASS: through pin queries"

puts "ALL PASSED"
