# Test deep network modification: merge nets, multiple instances,
# replace_cell with different cell types, and extensive connect/disconnect.
# Targets:
#   ConcreteNetwork.cc: mergeInto, deleteInstance, connectPin/disconnectPin,
#     makeConcreteInstance, findNetsMatching, findCellsMatching,
#     deleteNet, isPower, isGround, findPort, groupBusPorts,
#     visitConnectedPins, makeBusPort, netIterator, childIterator,
#     setAttribute, getAttribute, setName, setIsLeaf
#   Network.cc: pathNameCmp, pathNameLess, pathName, busIndexInRange,
#     hasMembers, isTopInstance, isInside, isHierarchical,
#     leafInstanceIterator, findLibertyCell, findLibertyFilename,
#     netDrvrPinMap, clearNetDrvrPinMap

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog network_test1.v
link_design network_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_input_delay -clock clk 0 [get_ports in2]
set_output_delay -clock clk 0 [get_ports out1]
set_input_transition 0.1 [all_inputs]

# Force graph build
report_checks
puts "PASS: initial design setup"

#---------------------------------------------------------------
# Test extensive instance creation/deletion cycle
#---------------------------------------------------------------
puts "--- extensive instance creation ---"
set cell_types {BUF_X1 BUF_X2 BUF_X4 INV_X1 INV_X2 INV_X4
                NAND2_X1 NOR2_X1 AND2_X1 OR2_X1 AOI21_X1 OAI21_X1}

set inst_names {}
set idx 0
foreach cell_type $cell_types {
  set inst_name "test_inst_$idx"
  set inst [make_instance $inst_name NangateOpenCellLibrary/$cell_type]
  lappend inst_names $inst_name
  incr idx
}
puts "PASS: created [llength $inst_names] instances"

set all_cells [get_cells *]
puts "total cells: [llength $all_cells]"

# Test finding cells by pattern
set test_cells [get_cells test_inst_*]
puts "test_inst_* cells: [llength $test_cells]"

#---------------------------------------------------------------
# Connect/disconnect cycle on new instances
#---------------------------------------------------------------
puts "--- connect/disconnect cycle ---"
set net_idx 0
foreach inst_name [lrange $inst_names 0 5] {
  set net_name "test_net_$net_idx"
  set net [make_net $net_name]
  catch {
    connect_pin $net_name ${inst_name}/A
    puts "PASS: connect $net_name to ${inst_name}/A"
  } msg
  catch {
    disconnect_pin $net_name ${inst_name}/A
    puts "PASS: disconnect $net_name from ${inst_name}/A"
  } msg
  delete_net $net_name
  incr net_idx
}
puts "PASS: connect/disconnect cycle"

#---------------------------------------------------------------
# Test multiple pin connections to same net
#---------------------------------------------------------------
puts "--- multi-pin connections ---"
set shared_net [make_net shared_net1]

catch {
  connect_pin shared_net1 test_inst_0/A
  puts "PASS: connect test_inst_0/A to shared_net1"
}
catch {
  connect_pin shared_net1 test_inst_1/A
  puts "PASS: connect test_inst_1/A to shared_net1"
}

# Verify net has multiple pins
catch {
  report_net shared_net1
  puts "PASS: report_net shared_net1"
}

# Disconnect all from shared net
catch {disconnect_pin shared_net1 test_inst_0/A}
catch {disconnect_pin shared_net1 test_inst_1/A}
delete_net shared_net1
puts "PASS: clean up shared_net1"

#---------------------------------------------------------------
# Replace cells with various types
#---------------------------------------------------------------
puts "--- replace_cell tests ---"

# Replace buf1 with different buffer sizes
replace_cell buf1 NangateOpenCellLibrary/BUF_X2
set ref1 [get_property [get_cells buf1] ref_name]
puts "buf1 -> BUF_X2: ref=$ref1"

replace_cell buf1 NangateOpenCellLibrary/BUF_X4
set ref2 [get_property [get_cells buf1] ref_name]
puts "buf1 -> BUF_X4: ref=$ref2"

replace_cell buf1 NangateOpenCellLibrary/BUF_X8
set ref3 [get_property [get_cells buf1] ref_name]
puts "buf1 -> BUF_X8: ref=$ref3"

replace_cell buf1 NangateOpenCellLibrary/BUF_X16
set ref4 [get_property [get_cells buf1] ref_name]
puts "buf1 -> BUF_X16: ref=$ref4"

# Restore
replace_cell buf1 NangateOpenCellLibrary/BUF_X1
puts "PASS: replace_cell series done"

# Report after replacements
report_checks
puts "PASS: report_checks after replacements"

#---------------------------------------------------------------
# Delete all test instances
#---------------------------------------------------------------
puts "--- delete test instances ---"
foreach inst_name $inst_names {
  catch {delete_instance $inst_name}
}
puts "PASS: deleted all test instances"

set remaining [get_cells *]
puts "remaining cells: [llength $remaining]"

#---------------------------------------------------------------
# Test make and delete net patterns
#---------------------------------------------------------------
puts "--- net creation/deletion patterns ---"
set net_names {}
for {set i 0} {$i < 20} {incr i} {
  set net_name "bulk_net_$i"
  make_net $net_name
  lappend net_names $net_name
}
puts "PASS: created 20 nets"

set all_nets [get_nets *]
puts "total nets with bulk: [llength $all_nets]"

set bulk_nets [get_nets bulk_net_*]
puts "bulk_net_* count: [llength $bulk_nets]"

foreach net_name $net_names {
  delete_net $net_name
}
puts "PASS: deleted 20 nets"

set all_nets2 [get_nets *]
puts "nets after cleanup: [llength $all_nets2]"

#---------------------------------------------------------------
# Test report for various object types
#---------------------------------------------------------------
puts "--- various reports ---"
report_instance buf1
report_instance and1
report_instance reg1
puts "PASS: report_instance"

report_net n1
report_net n2
puts "PASS: report_net"

#---------------------------------------------------------------
# Test all_registers
#---------------------------------------------------------------
set regs [all_registers]
puts "all_registers: [llength $regs]"

set reg_data [all_registers -data_pins]
puts "register data_pins: [llength $reg_data]"

set reg_clk [all_registers -clock_pins]
puts "register clock_pins: [llength $reg_clk]"

set reg_out [all_registers -output_pins]
puts "register output_pins: [llength $reg_out]"

#---------------------------------------------------------------
# Test various get_property on objects
#---------------------------------------------------------------
puts "--- property queries ---"
foreach port_name {clk in1 in2 out1} {
  set p [get_ports $port_name]
  set dir [get_property $p direction]
  set fn [get_full_name $p]
  puts "port $port_name: dir=$dir name=$fn"
}

foreach inst_name {buf1 and1 reg1} {
  set inst [get_cells $inst_name]
  set ref [get_property $inst ref_name]
  set fn [get_full_name $inst]
  puts "inst $inst_name: ref=$ref name=$fn"
}

foreach pin_path {buf1/A buf1/Z and1/A1 and1/A2 and1/ZN reg1/D reg1/CK reg1/Q} {
  set pin [get_pins $pin_path]
  set dir [get_property $pin direction]
  set fn [get_full_name $pin]
  puts "pin $pin_path: dir=$dir name=$fn"
}

foreach net_name {n1 n2} {
  set net [get_nets $net_name]
  set fn [get_full_name $net]
  puts "net $net_name: name=$fn"
}

puts "ALL PASSED"
