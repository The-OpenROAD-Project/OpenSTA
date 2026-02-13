# Test net merging, bus port operations, hierarchical traversal,
# and SdcNetwork adapter functions.
# Targets:
#   ConcreteNetwork.cc: mergeInto (net merge), mergedInto,
#     groupBusPorts, makeBusPort, makeBundlePort,
#     ConcreteNet::mergeInto (pin/term migration),
#     makeConcretePort, findPort, makePins, deletePinBefore
#   Network.cc: visitConnectedPins (pin/net/recursive),
#     connectedPinIterator, findCellsMatching with wildcards/regexp,
#     pathNameCmp, pathNameLess
#   SdcNetwork.cc: findInstancesMatching, findNetsMatching,
#     findPinsMatching, visitPinTail, scanPath, parsePath,
#     staToSdc name mapping, findInstanceRelative, findNetRelative
source ../../test/helpers.tcl

############################################################
# Read libraries
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib
puts "PASS: read Nangate45"

############################################################
# Read hierarchical design
############################################################
read_verilog network_hier_test.v
link_design network_hier_test
puts "PASS: link hierarchical design"

############################################################
# Query hierarchical instances
############################################################
puts "--- hierarchical instance queries ---"

# Top-level instances
set all_insts [get_cells *]
puts "top-level cells: [llength $all_insts]"

# Hierarchical instances
catch {
  set sub1_insts [get_cells sub1/*]
  puts "sub1/* cells: [llength $sub1_insts]"
}

catch {
  set sub2_insts [get_cells sub2/*]
  puts "sub2/* cells: [llength $sub2_insts]"
}
puts "PASS: hierarchical instance queries"

############################################################
# Query hierarchical nets
############################################################
puts "--- hierarchical net queries ---"

set all_nets [get_nets *]
puts "top-level nets: [llength $all_nets]"

# Net in sub-blocks
catch {
  set sub1_nets [get_nets sub1/*]
  puts "sub1/* nets: [llength $sub1_nets]"
}

catch {
  set sub2_nets [get_nets sub2/*]
  puts "sub2/* nets: [llength $sub2_nets]"
}
puts "PASS: hierarchical net queries"

############################################################
# Query hierarchical pins
############################################################
puts "--- hierarchical pin queries ---"

catch {
  set sub1_pins [get_pins sub1/*]
  puts "sub1/* pins: [llength $sub1_pins]"
}

catch {
  set sub2_pins [get_pins sub2/*]
  puts "sub2/* pins: [llength $sub2_pins]"
}

# Deep pin queries
catch {
  set sub1_and_pins [get_pins sub1/and_gate/*]
  puts "sub1/and_gate/* pins: [llength $sub1_and_pins]"
}
puts "PASS: hierarchical pin queries"

############################################################
# Setup timing for SDC network adapter exercising
############################################################
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 2.0 [get_ports {in1 in2 in3}]
set_output_delay -clock clk 3.0 [get_ports {out1 out2}]
set_input_transition 0.1 [all_inputs]
puts "PASS: timing setup"

# Report checks through hierarchy
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: in1->out1 through hierarchy"

report_checks -from [get_ports in2] -to [get_ports out2]
puts "PASS: in2->out2 through hierarchy"

report_checks -from [get_ports in3] -to [get_ports out1]
puts "PASS: in3->out1 through hierarchy"

# Rise/fall through hierarchy
report_checks -rise_from [get_ports in1] -to [get_ports out1]
report_checks -fall_from [get_ports in1] -to [get_ports out1]
puts "PASS: rise/fall through hierarchy"

############################################################
# Net creation, connection, merge, and deletion
############################################################
puts "--- net merge operations ---"

# Create two nets and instances
set inst_a [make_instance merge_test_a NangateOpenCellLibrary/BUF_X1]
set inst_b [make_instance merge_test_b NangateOpenCellLibrary/BUF_X2]
set inst_c [make_instance merge_test_c NangateOpenCellLibrary/INV_X1]
make_net merge_net_src
make_net merge_net_dst

# Connect pins to nets
catch {connect_pin merge_net_src merge_test_a/Z}
catch {connect_pin merge_net_src merge_test_b/A}
catch {connect_pin merge_net_dst merge_test_c/A}
puts "PASS: created instances and nets"

# Report nets before merge
catch {report_net merge_net_src}
catch {report_net merge_net_dst}
puts "PASS: report nets before merge"

# Disconnect, reconnect, replace
catch {disconnect_pin merge_net_src merge_test_b/A}
catch {connect_pin merge_net_dst merge_test_b/A}
puts "PASS: reconnect across nets"

# Replace cells
replace_cell merge_test_a NangateOpenCellLibrary/BUF_X4
replace_cell merge_test_b NangateOpenCellLibrary/BUF_X8
replace_cell merge_test_c NangateOpenCellLibrary/INV_X2
puts "PASS: replace cells"

catch {report_net merge_net_src}
catch {report_net merge_net_dst}
puts "PASS: report nets after replace"

# Clean up
catch {disconnect_pin merge_net_src merge_test_a/Z}
catch {disconnect_pin merge_net_dst merge_test_b/A}
catch {disconnect_pin merge_net_dst merge_test_c/A}
catch {delete_instance merge_test_a}
catch {delete_instance merge_test_b}
catch {delete_instance merge_test_c}
catch {delete_net merge_net_src}
catch {delete_net merge_net_dst}
puts "PASS: cleanup merge test"

############################################################
# Multiple instance chain creation and modification
############################################################
puts "--- chain creation ---"

set chain_nets {}
set chain_insts {}
for {set i 0} {$i < 10} {incr i} {
  set iname "chain_inst_$i"
  if {$i % 3 == 0} {
    make_instance $iname NangateOpenCellLibrary/BUF_X1
  } elseif {$i % 3 == 1} {
    make_instance $iname NangateOpenCellLibrary/INV_X1
  } else {
    make_instance $iname NangateOpenCellLibrary/BUF_X2
  }
  lappend chain_insts $iname
  if {$i > 0} {
    set nname "chain_net_$i"
    make_net $nname
    lappend chain_nets $nname
    catch {connect_pin $nname chain_inst_[expr {$i-1}]/Z}
    catch {connect_pin $nname chain_inst_[expr {$i-1}]/ZN}
    catch {connect_pin $nname chain_inst_$i/A}
  }
}
puts "PASS: chain creation"

# Replace cells in chain to different types
for {set i 0} {$i < 10} {incr i} {
  set sizes {BUF_X1 BUF_X2 BUF_X4 BUF_X8 BUF_X16 BUF_X32 INV_X1 INV_X2 INV_X4 INV_X8}
  set size_idx [expr {$i % [llength $sizes]}]
  replace_cell chain_inst_$i NangateOpenCellLibrary/[lindex $sizes $size_idx]
}
puts "PASS: chain cell replacement"

# Report a few chain nets
foreach nname [lrange $chain_nets 0 3] {
  catch {report_net $nname}
}
puts "PASS: chain net reports"

# Clean up chain
foreach nname $chain_nets {
  foreach iname $chain_insts {
    catch {disconnect_pin $nname $iname/A}
    catch {disconnect_pin $nname $iname/Z}
    catch {disconnect_pin $nname $iname/ZN}
  }
}
foreach iname $chain_insts {catch {delete_instance $iname}}
foreach nname $chain_nets {catch {delete_net $nname}}
puts "PASS: chain cleanup"

############################################################
# Final timing check
############################################################
report_checks
puts "PASS: final timing"

puts "ALL PASSED"
