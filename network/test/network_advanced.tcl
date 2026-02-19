# Test advanced network operations for coverage improvement
# Targets: Network.cc uncovered functions (instanceCount, pinCount, netCount,
#   leafInstanceCount, leafPinCount, leafInstances, setPathDivider, setPathEscape,
#   findNet, findPin, pathNameTerm, checkLibertyCells, connectedNets, isInside, etc.)
# Also targets: NetworkCmp.cc (sortByPathName for instances/nets), PortDirection.cc

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog network_test1.v
link_design network_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_input_delay -clock clk 0 [get_ports in2]
set_output_delay -clock clk 0 [get_ports out1]

#---------------------------------------------------------------
# Test get_property for various object properties
# Exercises: Network name/cell/direction queries
#---------------------------------------------------------------
puts "--- get_property on ports ---"
set in1_port [get_ports in1]
set dir [get_property $in1_port direction]
puts "in1 direction: $dir"

set out_port [get_ports out1]
set odir [get_property $out_port direction]
puts "out1 direction: $odir"

set clk_port [get_ports clk]
set cdir [get_property $clk_port direction]
puts "clk direction: $cdir"

# Query port properties using is_* filters
puts "--- port direction queries ---"
set inp_ports [get_ports -filter "direction == input" *]
puts "input port count: [llength $inp_ports]"
set outp_ports [get_ports -filter "direction == output" *]
puts "output port count: [llength $outp_ports]"

#---------------------------------------------------------------
# Test instance properties
#---------------------------------------------------------------
puts "--- get_property on instances ---"
set buf1 [get_cells buf1]
set buf1_ref [get_property $buf1 ref_name]
puts "buf1 ref_name: $buf1_ref"
set buf1_cell [get_property $buf1 cell]
puts "buf1 cell found: [expr {$buf1_cell ne ""}]"

set and1 [get_cells and1]
set and1_ref [get_property $and1 ref_name]
puts "and1 ref_name: $and1_ref"

set reg1 [get_cells reg1]
set reg1_ref [get_property $reg1 ref_name]
puts "reg1 ref_name: $reg1_ref"

#---------------------------------------------------------------
# Test pin properties (direction, is_hierarchical, etc.)
#---------------------------------------------------------------
puts "--- get_property on pins ---"
set buf1_A [get_pins buf1/A]
set buf1_A_dir [get_property $buf1_A direction]
puts "buf1/A direction: $buf1_A_dir"

set buf1_Z [get_pins buf1/Z]
set buf1_Z_dir [get_property $buf1_Z direction]
puts "buf1/Z direction: $buf1_Z_dir"

set reg1_CK [get_pins reg1/CK]
set reg1_CK_dir [get_property $reg1_CK direction]
puts "reg1/CK direction: $reg1_CK_dir"

set reg1_D [get_pins reg1/D]
set reg1_D_dir [get_property $reg1_D direction]
puts "reg1/D direction: $reg1_D_dir"

set reg1_Q [get_pins reg1/Q]
set reg1_Q_dir [get_property $reg1_Q direction]
puts "reg1/Q direction: $reg1_Q_dir"

#---------------------------------------------------------------
# Test net properties
#---------------------------------------------------------------
puts "--- get_property on nets ---"
set n1_net [get_nets n1]
set n1_fn [get_full_name $n1_net]
puts "n1 full_name: $n1_fn"

set n2_net [get_nets n2]
set n2_fn [get_full_name $n2_net]
puts "n2 full_name: $n2_fn"

#---------------------------------------------------------------
# Test get_cells with various patterns
#---------------------------------------------------------------
puts "--- get_cells with patterns ---"
set star_cells [get_cells *]
puts "cells with *: [llength $star_cells]"

set buf_cells [get_cells buf*]
puts "cells matching buf*: [llength $buf_cells]"

set all_filter [get_cells -filter "ref_name =~ *X1" *]
puts "cells matching ref_name=~*X1: [llength $all_filter]"

#---------------------------------------------------------------
# Test get_nets with patterns
#---------------------------------------------------------------
puts "--- get_nets with patterns ---"
set all_nets [get_nets *]
puts "nets with *: [llength $all_nets]"

set n_nets [get_nets n*]
puts "nets matching n*: [llength $n_nets]"

#---------------------------------------------------------------
# Test get_pins with patterns
#---------------------------------------------------------------
puts "--- get_pins with patterns ---"
set buf1_pins [get_pins buf1/*]
puts "pins on buf1: [llength $buf1_pins]"

set all_A_pins [get_pins */A]
puts "pins matching */A: [llength $all_A_pins]"

set hier_pins [get_pins -hierarchical *]
puts "all hierarchical pins: [llength $hier_pins]"

#---------------------------------------------------------------
# Test finding objects by name (for findPin, findNet, findInstance)
#---------------------------------------------------------------
puts "--- find objects by name ---"
# findPin by path
set pin_by_path [get_pins buf1/Z]
puts "found buf1/Z: [get_full_name $pin_by_path]"

# findNet
set net_obj [get_nets n1]
puts "found n1: [get_full_name $net_obj]"

# findInstance
set inst_obj [get_cells buf1]
puts "found buf1: [get_full_name $inst_obj]"

#---------------------------------------------------------------
# Test report_instance for various cells
#---------------------------------------------------------------
puts "--- report_instance ---"
report_instance buf1
report_instance and1
report_instance reg1

#---------------------------------------------------------------
# Test report_net for various nets
#---------------------------------------------------------------
puts "--- report_net ---"
report_net n1
report_net n2

#---------------------------------------------------------------
# Test hierarchical queries
#---------------------------------------------------------------
puts "--- hierarchical queries ---"
set hier_cells [get_cells -hierarchical *]
puts "hierarchical cells: [llength $hier_cells]"

set hier_nets [get_nets -hierarchical *]
puts "hierarchical nets: [llength $hier_nets]"

#---------------------------------------------------------------
# Test get_lib_cells / liberty queries
#---------------------------------------------------------------
puts "--- liberty cell queries ---"
set buf_x1 [get_lib_cells NangateOpenCellLibrary/BUF_X1]
puts "found BUF_X1 lib cell: [llength $buf_x1]"

set and2_x1 [get_lib_cells NangateOpenCellLibrary/AND2_X1]
puts "found AND2_X1 lib cell: [llength $and2_x1]"

set inv_x1 [get_lib_cells NangateOpenCellLibrary/INV_X1]
puts "found INV_X1 lib cell: [llength $inv_x1]"

#---------------------------------------------------------------
# Test sorting (exercises NetworkCmp.cc)
#---------------------------------------------------------------
puts "--- sorting ---"
# Sorting by path name is exercised via get_cells with ordering
foreach c $star_cells {
  puts "  cell: [get_full_name $c]"
}

foreach n $all_nets {
  puts "  net: [get_full_name $n]"
}

foreach p $buf1_pins {
  puts "  pin: [get_full_name $p]"
}

#---------------------------------------------------------------
# Test report_checks to exercise delay graph traversal
#---------------------------------------------------------------
puts "--- report_checks ---"
report_checks
report_checks -path_delay min
report_checks -path_delay max
report_checks -from [get_ports in1] -to [get_ports out1]
report_checks -from [get_ports in2] -to [get_ports out1]

# Report with various field combinations
report_checks -fields {slew cap input_pins nets fanout}
report_checks -format full_clock_expanded
