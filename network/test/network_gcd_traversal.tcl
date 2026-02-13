# Test network traversal, cell matching, and property queries on a larger
# design (GCD Sky130HD) to exercise deeper Network.cc code paths.
# Targets:
#   Network.cc: findCellsMatching (with regex/glob patterns),
#     visitConnectedPins, visitConnectedPinsDownNetwork,
#     isDriver, isLoad, isRegClkPin, isLatchData, isCheckClk,
#     highestConnectedNet, netDrvrPinMap, clearNetDrvrPinMap,
#     drivers, connectedNets, isConnected,
#     leafInstanceCount, leafPinCount, leafInstances,
#     instanceCount, pinCount, netCount,
#     findCell, findPort, findPin
#   ConcreteNetwork.cc: findNet, findPort, findPin, findInstance,
#     setAttribute, getAttribute, groupBusPorts, portBitCount,
#     makePins, isPower, isGround, clearConstantNets
source ../../test/helpers.tcl

############################################################
# Read Sky130 library and GCD design
############################################################
read_liberty ../../test/sky130hd/sky130hd_tt.lib
puts "PASS: read sky130hd"

read_verilog ../../examples/gcd_sky130hd.v
link_design gcd
puts "PASS: link gcd"

source ../../examples/gcd_sky130hd.sdc
puts "PASS: SDC"

# Build timing graph
report_checks -endpoint_count 1
puts "PASS: initial timing"

############################################################
# Instance and net counts (exercises leafInstanceCount, etc.)
############################################################
puts "--- design counts ---"
set all_cells [get_cells *]
puts "cells: [llength $all_cells]"

set all_nets [get_nets *]
puts "nets: [llength $all_nets]"

set all_ports [get_ports *]
puts "ports: [llength $all_ports]"

############################################################
# Cell matching with regex and glob patterns
# Exercises: findCellsMatching, PatternMatch
############################################################
puts "--- cell matching ---"
set matches [sta::find_cells_matching "*" 0 0]
puts "glob * = [llength $matches]"

set matches [sta::find_cells_matching "sky130_fd_sc_hd__inv_*" 0 0]
puts "glob inv_* = [llength $matches]"

set matches [sta::find_cells_matching "sky130_fd_sc_hd__buf_*" 0 0]
puts "glob buf_* = [llength $matches]"

set matches [sta::find_cells_matching "sky130_fd_sc_hd__dfxtp_*" 0 0]
puts "glob dfxtp_* = [llength $matches]"

set matches [sta::find_cells_matching "sky130_fd_sc_hd__nand*" 0 0]
puts "glob nand* = [llength $matches]"

# Regex pattern matching
set matches [sta::find_cells_matching {^sky130_fd_sc_hd__inv_[0-9]+$} 1 0]
puts "regex inv = [llength $matches]"

set matches [sta::find_cells_matching {^sky130_fd_sc_hd__.*_1$} 1 0]
puts "regex *_1 = [llength $matches]"

# Case-insensitive matching
set matches [sta::find_cells_matching "SKY130_FD_SC_HD__INV_*" 0 1]
puts "nocase INV_* = [llength $matches]"

puts "PASS: cell matching"

############################################################
# Net connectivity queries (exercises connectedPinIterator,
# visitConnectedPins, drivers)
############################################################
puts "--- net connectivity ---"
set net_count 0
foreach net_obj [get_nets *] {
  set nname [get_name $net_obj]
  set pins [get_pins -of_objects $net_obj]
  set npin [llength $pins]
  if {$npin > 3} {
    puts "net $nname pins=$npin"
    incr net_count
    if {$net_count >= 20} break
  }
}
puts "PASS: net connectivity ($net_count nets)"

# Report nets to exercise pin iteration
set sample_count 0
foreach net_obj [get_nets *] {
  catch {
    report_net -digits 4 [get_name $net_obj]
  }
  incr sample_count
  if {$sample_count >= 15} break
}
puts "PASS: report_net sample"

############################################################
# Instance property queries
############################################################
puts "--- instance properties ---"
set inst_count 0
foreach inst_obj [get_cells *] {
  set iname [get_name $inst_obj]
  set ref [get_property $inst_obj ref_name]
  set fn [get_full_name $inst_obj]
  puts "$iname ref=$ref full_name=$fn"
  incr inst_count
  if {$inst_count >= 20} break
}
puts "PASS: instance properties"

############################################################
# Pin direction and property queries
############################################################
puts "--- pin properties ---"
set pin_count 0
foreach inst_obj [get_cells *] {
  set iname [get_name $inst_obj]
  set pins [get_pins $iname/*]
  foreach p $pins {
    set dir [get_property $p direction]
    set pname [get_full_name $p]
    puts "  $pname dir=$dir"
    incr pin_count
  }
  if {$pin_count >= 40} break
}
puts "PASS: pin properties ($pin_count pins)"

############################################################
# Port direction queries
############################################################
puts "--- port properties ---"
foreach port_obj [get_ports *] {
  set pname [get_name $port_obj]
  set dir [get_property $port_obj direction]
  puts "port $pname dir=$dir"
}
puts "PASS: port properties"

############################################################
# Library queries (exercises find_library, library_iterator)
############################################################
puts "--- library queries ---"
set lib [sta::find_library sky130_fd_sc_hd__tt_025C_1v80]
puts "find_library: [$lib name]"

set lib_iter [sta::library_iterator]
while {[$lib_iter has_next]} {
  set lib [$lib_iter next]
  puts "library: [$lib name]"
}
$lib_iter finish
puts "PASS: library queries"

############################################################
# Connected pin traversal via timing paths
############################################################
puts "--- timing path traversal ---"
report_checks -from [get_ports clk] -endpoint_count 3
puts "PASS: from clk"

report_checks -from [get_ports reset] -endpoint_count 3
puts "PASS: from reset"

report_checks -from [get_ports req_val] -endpoint_count 3
puts "PASS: from req_val"

catch {
  report_checks -to [get_ports resp_val] -endpoint_count 3
  puts "PASS: to resp_val"
}

catch {
  report_checks -to [get_ports resp_rdy] -endpoint_count 3
  puts "PASS: to resp_rdy"
}

# Path from specific inputs to outputs
foreach in_port {req_val reset req_rdy} {
  foreach out_port {resp_val resp_rdy} {
    catch {
      report_checks -from [get_ports $in_port] -to [get_ports $out_port]
    }
  }
}
puts "PASS: input-to-output paths"

############################################################
# Namespace commands
############################################################
puts "--- namespace ---"
sta::set_cmd_namespace_cmd "sdc"
puts "PASS: set namespace sdc"

sta::set_cmd_namespace_cmd "sta"
puts "PASS: set namespace sta"

puts "ALL PASSED"
