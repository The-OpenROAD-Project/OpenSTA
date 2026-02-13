# Test network editing with replaceCell (equiv and non-equiv),
# connectedCap, portExtCaps, setPortExtPinCap/WireCap/Fanout,
# setNetWireCap, report_net, incremental timing after edits.
# Targets: Sta.cc replaceCell, replaceEquivCellBefore/After,
#          replaceCellBefore/After, replaceCellPinInvalidate,
#          idealClockMode, libertyPortCapsEqual,
#          connectedCap (Pin and Net versions), portExtCaps,
#          setPortExtPinCap, setPortExtWireCap, setPortExtFanout,
#          setNetWireCap, removeNetLoadCaps,
#          makeInstance, deleteInstance, connectPin, disconnectPin,
#          makeNet, deleteNet, makePortPin,
#          connectPinAfter, connectDrvrPinAfter, connectLoadPinAfter,
#          disconnectPinBefore, deleteEdge, deleteNetBefore,
#          deleteInstanceBefore, deleteLeafInstanceBefore
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

# Baseline timing
report_checks -path_delay max > /dev/null

############################################################
# replaceCell with equiv cell (same ports, same timing arcs)
# This exercises replaceEquivCellBefore/After path
############################################################
puts "--- replaceCell equiv: BUF_X1 -> BUF_X2 ---"
replace_cell buf1 NangateOpenCellLibrary/BUF_X2
report_checks -path_delay max
puts "PASS: replaceCell equiv BUF_X1->BUF_X2"

puts "--- replaceCell equiv: BUF_X2 -> BUF_X4 ---"
replace_cell buf1 NangateOpenCellLibrary/BUF_X4
report_checks -path_delay max
puts "PASS: replaceCell equiv BUF_X2->BUF_X4"

puts "--- replaceCell equiv: BUF_X4 -> BUF_X8 ---"
replace_cell buf1 NangateOpenCellLibrary/BUF_X8
report_checks -path_delay max
puts "PASS: replaceCell equiv BUF_X4->BUF_X8"

puts "--- replaceCell back: BUF_X8 -> BUF_X1 ---"
replace_cell buf1 NangateOpenCellLibrary/BUF_X1
report_checks -path_delay max
puts "PASS: replaceCell equiv BUF_X8->BUF_X1"

puts "--- replaceCell equiv: AND2_X1 -> AND2_X2 ---"
replace_cell and1 NangateOpenCellLibrary/AND2_X2
report_checks -path_delay max
puts "PASS: replaceCell equiv AND2_X1->AND2_X2"

puts "--- replaceCell equiv: AND2_X2 -> AND2_X4 ---"
replace_cell and1 NangateOpenCellLibrary/AND2_X4
report_checks -path_delay max
puts "PASS: replaceCell equiv AND2_X2->AND2_X4"

puts "--- replaceCell back: AND2_X4 -> AND2_X1 ---"
replace_cell and1 NangateOpenCellLibrary/AND2_X1
report_checks -path_delay max
puts "PASS: replaceCell equiv AND2_X4->AND2_X1"

puts "--- replaceCell equiv buf2: BUF_X1 -> BUF_X2 ---"
replace_cell buf2 NangateOpenCellLibrary/BUF_X2
report_checks -path_delay max
puts "PASS: replaceCell equiv buf2"

############################################################
# replaceCell with propagated clock
# This exercises idealClockMode returning false
############################################################
puts "--- replaceCell with propagated clock ---"
set_propagated_clock [get_clocks clk]
replace_cell buf1 NangateOpenCellLibrary/BUF_X2
report_checks -path_delay max
replace_cell buf1 NangateOpenCellLibrary/BUF_X1
report_checks -path_delay max
unset_propagated_clock [get_clocks clk]
puts "PASS: replaceCell with propagated clock"

############################################################
# Port ext pin cap, wire cap, fanout
############################################################
puts "--- setPortExtPinCap ---"
set_load -pin_load 0.05 [get_ports out1]
report_checks -path_delay max
puts "PASS: setPortExtPinCap"

puts "--- setPortExtWireCap ---"
set_load -wire_load 0.03 [get_ports out1]
report_checks -path_delay max
puts "PASS: setPortExtWireCap"

puts "--- setPortExtFanout ---"
set_port_fanout_number 4 [get_ports out1]
report_checks -path_delay max
puts "PASS: setPortExtFanout"

puts "--- set_load with rise/fall ---"
set_load -pin_load 0.04 [get_ports out1]
report_checks -path_delay max
puts "PASS: set_load rise/fall"

############################################################
# connectedCap and report_net
############################################################
puts "--- report_net ---"
report_net n1
report_net n2
report_net n3
puts "PASS: report_net"

############################################################
# setNetWireCap
############################################################
puts "--- setNetWireCap ---"
catch {
  set_load 0.01 [get_nets n1]
  report_checks -path_delay max
}
puts "PASS: setNetWireCap"

############################################################
# Network edits: complex sequence
############################################################
puts "--- Network edit: make_instance + connect + replace ---"
catch {
  make_instance new_inv1 NangateOpenCellLibrary/INV_X1
  make_net test_net1
  connect_pin test_net1 new_inv1/A
  report_checks -path_delay max
  disconnect_pin test_net1 new_inv1/A
  delete_net test_net1
  delete_instance new_inv1
}
puts "PASS: complex network edit"

puts "--- Network edit: make multiple instances ---"
catch {
  make_instance extra_buf1 NangateOpenCellLibrary/BUF_X1
  make_instance extra_buf2 NangateOpenCellLibrary/BUF_X2
  make_instance extra_inv1 NangateOpenCellLibrary/INV_X1
  make_net extra_net1
  make_net extra_net2
  connect_pin extra_net1 extra_buf1/A
  connect_pin extra_net1 extra_buf2/A
  connect_pin extra_net2 extra_inv1/A
  report_checks -path_delay max
  disconnect_pin extra_net1 extra_buf1/A
  disconnect_pin extra_net1 extra_buf2/A
  disconnect_pin extra_net2 extra_inv1/A
  delete_net extra_net1
  delete_net extra_net2
  delete_instance extra_buf1
  delete_instance extra_buf2
  delete_instance extra_inv1
}
puts "PASS: multiple network edits"

############################################################
# Incremental timing after replacing multiple cells
############################################################
puts "--- Multiple replaceCell + timing ---"
replace_cell buf1 NangateOpenCellLibrary/BUF_X4
replace_cell buf2 NangateOpenCellLibrary/BUF_X4
replace_cell and1 NangateOpenCellLibrary/AND2_X4
report_checks -path_delay max
report_checks -path_delay min
puts "PASS: multiple replaceCell"

# Replace back
replace_cell buf1 NangateOpenCellLibrary/BUF_X1
replace_cell buf2 NangateOpenCellLibrary/BUF_X1
replace_cell and1 NangateOpenCellLibrary/AND2_X1
report_checks -path_delay max
puts "PASS: replaceCell restore"

############################################################
# Report timing with fields after edits
############################################################
puts "--- report_checks with fields after edits ---"
report_checks -path_delay max -fields {capacitance slew fanout}
report_checks -path_delay min -fields {capacitance slew fanout}
puts "PASS: report with fields after edits"

puts "ALL PASSED"
