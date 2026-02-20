# Test Sta.cc: deleteInstanceBefore with timing active, non-equiv replaceCell,
# connectPin/disconnectPin sequences with timing updates,
# setPortExtPinCap/setPortExtWireCap with rise/fall corners,
# makePortPin, incremental timing invalidation after net editing,
# bidirect path enable/disable, slow_drivers with larger count,
# set_case_analysis with timing updates.
# Targets: Sta.cc deleteInstanceBefore, replaceCellBefore/After (non-equiv),
#          connectPinAfter/connectDrvrPinAfter/connectLoadPinAfter,
#          disconnectPinBefore, deleteNetBefore, deleteLeafInstanceBefore,
#          setBidirectInstPathsEnabled, setBidirectNetPathsEnabled,
#          slowDrivers, setPortExtPinCap, setPortExtWireCap
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
# Non-equiv replaceCell: AND2_X1 -> OR2_X1 (different function)
# This exercises replaceCellBefore/After path (not equiv)
############################################################
puts "--- Non-equiv replaceCell: AND2_X1 -> OR2_X1 ---"
replace_cell and1 NangateOpenCellLibrary/OR2_X1
report_checks -path_delay max

puts "--- Non-equiv replaceCell: OR2_X1 -> NAND2_X1 ---"
replace_cell and1 NangateOpenCellLibrary/NAND2_X1
report_checks -path_delay max

puts "--- Non-equiv replaceCell: NAND2_X1 -> NOR2_X1 ---"
replace_cell and1 NangateOpenCellLibrary/NOR2_X1
report_checks -path_delay max

puts "--- Restore to AND2_X1 ---"
replace_cell and1 NangateOpenCellLibrary/AND2_X1
report_checks -path_delay max

############################################################
# slow_drivers with larger count
############################################################
puts "--- slow_drivers 5 ---"
set slow5 [sta::slow_drivers 5]
puts "slow_drivers(5): [llength $slow5]"
foreach s $slow5 {
  if { $s != "NULL" } {
    puts "  [get_full_name $s]"
  }
}

############################################################
# Bidirectional path enable/disable
############################################################
puts "--- bidirect_inst_paths ---"
set orig_bidir_inst [sta::bidirect_inst_paths_enabled]
puts "bidirect_inst_paths_enabled: $orig_bidir_inst"
sta::set_bidirect_inst_paths_enabled 0
puts "After disable: [sta::bidirect_inst_paths_enabled]"
report_checks -path_delay max > /dev/null
sta::set_bidirect_inst_paths_enabled 1
puts "After enable: [sta::bidirect_inst_paths_enabled]"
report_checks -path_delay max > /dev/null
sta::set_bidirect_inst_paths_enabled $orig_bidir_inst

puts "--- bidirect_net_paths ---"
set orig_bidir_net [sta::bidirect_net_paths_enabled]
puts "bidirect_net_paths_enabled: $orig_bidir_net"
sta::set_bidirect_net_paths_enabled 0
puts "After disable: [sta::bidirect_net_paths_enabled]"
report_checks -path_delay max > /dev/null
sta::set_bidirect_net_paths_enabled 1
puts "After enable: [sta::bidirect_net_paths_enabled]"
report_checks -path_delay max > /dev/null
sta::set_bidirect_net_paths_enabled $orig_bidir_net

############################################################
# Complex network edit sequence: make, connect, verify timing,
# disconnect, delete (exercises deleteInstanceBefore with timing)
############################################################
puts "--- Complex network edit with timing ---"
# Create new buffer instance
set new_buf [make_instance extra_buf NangateOpenCellLibrary/BUF_X1]
puts "Created: [get_full_name $new_buf]"

# Create new net
set new_net [make_net extra_net]
puts "Created net: [get_full_name $new_net]"

# Connect input
connect_pin $new_net extra_buf/A
puts "Connected A"

# Connect output
connect_pin $new_net extra_buf/Z
puts "Connected Z"

# Run timing (exercises timing update with new instance)
report_checks -path_delay max > /dev/null
puts "Timing after connect"

# Disconnect
disconnect_pin $new_net extra_buf/Z
puts "Disconnected Z"

disconnect_pin $new_net extra_buf/A
puts "Disconnected A"

# Run timing after disconnect
report_checks -path_delay max > /dev/null
puts "Timing after disconnect"

# Delete instance (exercises deleteInstanceBefore)
delete_instance $new_buf
puts "Deleted instance"

# Delete net (exercises deleteNetBefore)
delete_net $new_net
puts "Deleted net"

# Verify timing still works
report_checks -path_delay max

############################################################
# Multiple instance creation and deletion
############################################################
puts "--- Multiple instance create/delete ---"
set inst1 [make_instance tmp_buf1 NangateOpenCellLibrary/BUF_X1]
set inst2 [make_instance tmp_inv1 NangateOpenCellLibrary/INV_X1]
set inst3 [make_instance tmp_and1 NangateOpenCellLibrary/AND2_X1]
set net1 [make_net tmp_net1]
set net2 [make_net tmp_net2]

connect_pin $net1 tmp_buf1/A
connect_pin $net1 tmp_inv1/A
connect_pin $net2 tmp_and1/A1

report_checks -path_delay max > /dev/null

disconnect_pin $net1 tmp_buf1/A
disconnect_pin $net1 tmp_inv1/A
disconnect_pin $net2 tmp_and1/A1

delete_instance $inst1
delete_instance $inst2
delete_instance $inst3
delete_net $net1
delete_net $net2

report_checks -path_delay max

############################################################
# setPortExtPinCap with rise/fall, multiple values
############################################################
puts "--- setPortExtPinCap multiple ---"
set_load -pin_load 0.02 [get_ports out1]
report_checks -path_delay max -fields {capacitance}
puts "After pin_load 0.02"

set_load -pin_load 0.08 [get_ports out1]
report_checks -path_delay max -fields {capacitance}
puts "After pin_load 0.08"

set_load -pin_load 0.0 [get_ports out1]
report_checks -path_delay max -fields {capacitance}

############################################################
# setPortExtWireCap
############################################################
puts "--- setPortExtWireCap ---"
set_load -wire_load 0.02 [get_ports out1]
report_checks -path_delay max -fields {capacitance}
puts "After wire_load 0.02"

set_load -wire_load 0.06 [get_ports out1]
report_checks -path_delay max -fields {capacitance}
puts "After wire_load 0.06"

set_load -wire_load 0.0 [get_ports out1]
report_checks -path_delay max -fields {capacitance}

############################################################
# set_port_fanout_number
############################################################
puts "--- set_port_fanout_number ---"
set_port_fanout_number 8 [get_ports out1]
report_checks -path_delay max
set_port_fanout_number 1 [get_ports out1]
report_checks -path_delay max

############################################################
# report_net after edits
############################################################
puts "--- report_net ---"
report_net n1
report_net n2
report_net n3

############################################################
# Replace cell multiple times, verify incremental timing
############################################################
puts "--- Rapid replaceCell sequence ---"
replace_cell buf1 NangateOpenCellLibrary/BUF_X2
replace_cell buf2 NangateOpenCellLibrary/BUF_X2
report_checks -path_delay max > /dev/null
replace_cell buf1 NangateOpenCellLibrary/BUF_X4
replace_cell buf2 NangateOpenCellLibrary/BUF_X4
report_checks -path_delay max > /dev/null
replace_cell buf1 NangateOpenCellLibrary/BUF_X8
replace_cell buf2 NangateOpenCellLibrary/BUF_X8
report_checks -path_delay max
replace_cell buf1 NangateOpenCellLibrary/BUF_X1
replace_cell buf2 NangateOpenCellLibrary/BUF_X1
report_checks -path_delay max

############################################################
# set_case_analysis and verify timing update
############################################################
puts "--- set_case_analysis ---"
set_case_analysis 1 [get_ports in2]
report_checks -path_delay max
unset_case_analysis [get_ports in2]
report_checks -path_delay max

############################################################
# disable_timing and verify
############################################################
puts "--- disable_timing ---"
set_disable_timing [get_lib_cells NangateOpenCellLibrary/BUF_X1] -from A -to Z
report_checks -path_delay max
report_disabled_edges
unset_disable_timing [get_lib_cells NangateOpenCellLibrary/BUF_X1] -from A -to Z
report_checks -path_delay max
