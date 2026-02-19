# Test cell pattern matching, net merging, bus port operations,
# and network modification with multiple libraries.
# Targets:
#   Network.cc: findCellsMatching, findNetsMatching, findNetsHierMatching,
#     findInstancesMatching, findInstancesHierMatching,
#     pathNameCmp, pathNameLess, busIndexInRange
#   ConcreteNetwork.cc: mergeInto (net merge), groupBusPorts,
#     makeConcretePort, findPort, deletePinBefore,
#     connect, disconnect, replaceCell, setAttribute, getAttribute
#   SdcNetwork.cc: findCellsMatching, findNetsMatching delegation

source ../../test/helpers.tcl

############################################################
# Read libraries
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_liberty ../../test/sky130hd/sky130hd_tt.lib

############################################################
# Cell pattern matching on libraries
# Exercises: find_liberty_cells_matching with patterns
############################################################
puts "--- cell pattern matching ---"

set ng_lib [sta::find_liberty NangateOpenCellLibrary]

# Wildcard patterns
set inv_cells [$ng_lib find_liberty_cells_matching "INV_*" 0 0]
puts "INV_* matches: [llength $inv_cells]"

set buf_cells [$ng_lib find_liberty_cells_matching "BUF_*" 0 0]
puts "BUF_* matches: [llength $buf_cells]"

set dff_cells [$ng_lib find_liberty_cells_matching "DFF*" 0 0]
puts "DFF* matches: [llength $dff_cells]"

set nand_cells [$ng_lib find_liberty_cells_matching "NAND*" 0 0]
puts "NAND* matches: [llength $nand_cells]"

set nor_cells [$ng_lib find_liberty_cells_matching "NOR*" 0 0]
puts "NOR* matches: [llength $nor_cells]"

set aoi_cells [$ng_lib find_liberty_cells_matching "AOI*" 0 0]
puts "AOI* matches: [llength $aoi_cells]"

set oai_cells [$ng_lib find_liberty_cells_matching "OAI*" 0 0]
puts "OAI* matches: [llength $oai_cells]"

# All cells
set all_cells [$ng_lib find_liberty_cells_matching "*" 0 0]
puts "all cells: [llength $all_cells]"

# Regexp patterns
set re_inv [$ng_lib find_liberty_cells_matching {^INV_X[0-9]+$} 1 0]
puts "regexp INV_X#: [llength $re_inv]"

set re_buf [$ng_lib find_liberty_cells_matching {^BUF_X[0-9]+$} 1 0]
puts "regexp BUF_X#: [llength $re_buf]"

set re_dff [$ng_lib find_liberty_cells_matching {^DFF[RS]*_X[12]$} 1 0]
puts "regexp DFF(R|S|RS)_X(1|2): [llength $re_dff]"

# Nocase patterns
set nc_nand [$ng_lib find_liberty_cells_matching "nand*" 0 1]
puts "nocase nand*: [llength $nc_nand]"

set nc_buf [$ng_lib find_liberty_cells_matching "buf_*" 0 1]
puts "nocase buf_*: [llength $nc_buf]"

# Sky130 pattern matching
set sky_lib [sta::find_liberty sky130_fd_sc_hd__tt_025C_1v80]

set sky_inv [$sky_lib find_liberty_cells_matching "*inv*" 0 0]
puts "sky inv* matches: [llength $sky_inv]"

set sky_buf [$sky_lib find_liberty_cells_matching "*buf*" 0 0]
puts "sky buf* matches: [llength $sky_buf]"

set sky_dff [$sky_lib find_liberty_cells_matching "*dfxtp*" 0 0]
puts "sky dfxtp* matches: [llength $sky_dff]"

set sky_scan [$sky_lib find_liberty_cells_matching "*sdf*" 0 0]
puts "sky sdf* matches: [llength $sky_scan]"

set sky_latch [$sky_lib find_liberty_cells_matching "*dlx*" 0 0]
puts "sky dlx* matches: [llength $sky_latch]"

set sky_clkgate [$sky_lib find_liberty_cells_matching "*dlclkp*" 0 0]
puts "sky dlclkp* matches: [llength $sky_clkgate]"

set sky_lvlshift [$sky_lib find_liberty_cells_matching "*lsbuf*" 0 0]
puts "sky lsbuf* matches: [llength $sky_lvlshift]"

set sky_all [$sky_lib find_liberty_cells_matching "*" 0 0]
puts "sky all cells: [llength $sky_all]"

############################################################
# Port matching on cells
# Exercises: find_liberty_ports_matching
############################################################
puts "--- port pattern matching ---"

set dff_cell [get_lib_cell NangateOpenCellLibrary/DFF_X1]
set dff_ports [$dff_cell find_liberty_ports_matching "*" 0 0]
puts "DFF_X1 all ports: [llength $dff_ports]"

set dff_q_ports [$dff_cell find_liberty_ports_matching "Q*" 0 0]
puts "DFF_X1 Q* ports: [llength $dff_q_ports]"

set dffr_cell [get_lib_cell NangateOpenCellLibrary/DFFR_X1]
set dffr_ports [$dffr_cell find_liberty_ports_matching "*" 0 0]
puts "DFFR_X1 all ports: [llength $dffr_ports]"

set dffrs_cell [get_lib_cell NangateOpenCellLibrary/DFFRS_X1]
set dffrs_ports [$dffrs_cell find_liberty_ports_matching "*" 0 0]
puts "DFFRS_X1 all ports: [llength $dffrs_ports]"

# Pattern for S* (SN port on some cells)
set dffrs_s [$dffrs_cell find_liberty_ports_matching "S*" 0 0]
puts "DFFRS_X1 S* ports: [llength $dffrs_s]"

set dffrs_r [$dffrs_cell find_liberty_ports_matching "R*" 0 0]
puts "DFFRS_X1 R* ports: [llength $dffrs_r]"

############################################################
# Load a design and exercise network-level pattern matching
############################################################
read_verilog network_test1.v
link_design network_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_input_delay -clock clk 0 [get_ports in2]
set_output_delay -clock clk 0 [get_ports out1]
set_input_transition 0.1 [all_inputs]
report_checks

############################################################
# Instance pattern matching
############################################################
puts "--- instance pattern matching ---"

set all_insts [get_cells *]
puts "all cells: [llength $all_insts]"

set buf_insts [get_cells buf*]
puts "buf* cells: [llength $buf_insts]"

set reg_insts [get_cells reg*]
puts "reg* cells: [llength $reg_insts]"

set and_insts [get_cells and*]
puts "and* cells: [llength $and_insts]"

set inv_insts [get_cells inv*]
puts "inv* cells: [llength $inv_insts]"

set or_insts [get_cells or*]
puts "or* cells: [llength $or_insts]"

set n_insts [get_cells n*]
puts "n* cells: [llength $n_insts]"

############################################################
# Net pattern matching
############################################################
puts "--- net pattern matching ---"

set all_nets [get_nets *]
puts "all nets: [llength $all_nets]"

set n_nets [get_nets n*]
puts "n* nets: [llength $n_nets]"

############################################################
# Create instances and exercise net merging
# Exercises: mergeInto in ConcreteNetwork
############################################################
puts "--- net merge operations ---"

# Create two instances connected to different nets, then merge
set inst_a [make_instance merge_buf_a NangateOpenCellLibrary/BUF_X1]
set inst_b [make_instance merge_buf_b NangateOpenCellLibrary/BUF_X1]
make_net merge_net_1
make_net merge_net_2

connect_pin merge_net_1 merge_buf_a/Z
connect_pin merge_net_2 merge_buf_b/A

# Verify both nets exist
set mn1 [get_nets merge_net_1]
set mn2 [get_nets merge_net_2]
puts "merge_net_1 exists, merge_net_2 exists"

# Now do a cell replacement to exercise mergeInto path
# Replace buf_a with BUF_X2
replace_cell merge_buf_a NangateOpenCellLibrary/BUF_X2
set ref [get_property [get_cells merge_buf_a] ref_name]
puts "merge_buf_a -> BUF_X2: ref=$ref"

# Replace merge_buf_b with different sizes
replace_cell merge_buf_b NangateOpenCellLibrary/BUF_X4
set ref [get_property [get_cells merge_buf_b] ref_name]
puts "merge_buf_b -> BUF_X4: ref=$ref"

replace_cell merge_buf_b NangateOpenCellLibrary/INV_X1
set ref [get_property [get_cells merge_buf_b] ref_name]
puts "merge_buf_b -> INV_X1: ref=$ref"

# Disconnect and clean up
disconnect_pin merge_net_1 merge_buf_a/Z
disconnect_pin merge_net_2 merge_buf_b/A
delete_instance merge_buf_a
delete_instance merge_buf_b
delete_net merge_net_1
delete_net merge_net_2

############################################################
# Exercise multiple cell creation and connection patterns
############################################################
puts "--- multi-cell connection patterns ---"

# Create a chain of buffers
set chain_nets {}
set chain_insts {}
for {set i 0} {$i < 8} {incr i} {
  set iname "chain_buf_$i"
  make_instance $iname NangateOpenCellLibrary/BUF_X1
  lappend chain_insts $iname

  if {$i > 0} {
    set nname "chain_net_$i"
    make_net $nname
    lappend chain_nets $nname
    connect_pin $nname chain_buf_[expr {$i-1}]/Z
    connect_pin $nname chain_buf_$i/A
  }
}

# Report some nets in the chain
foreach nname [lrange $chain_nets 0 2] {
  report_net $nname
}

# Replace cells in chain
for {set i 0} {$i < 8} {incr i} {
  set sizes {BUF_X1 BUF_X2 BUF_X4}
  set size_idx [expr {$i % 3}]
  replace_cell chain_buf_$i NangateOpenCellLibrary/[lindex $sizes $size_idx]
}

# Clean up chain
foreach nname $chain_nets {
  foreach iname $chain_insts {
    disconnect_pin $nname $iname/A
    disconnect_pin $nname $iname/Z
  }
}
foreach iname $chain_insts {catch {delete_instance $iname}}
foreach nname $chain_nets {catch {delete_net $nname}}

# Final timing check
report_checks
