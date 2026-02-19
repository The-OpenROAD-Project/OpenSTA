# Deep timing arc and model queries exercising uncovered code paths.
# Targets:
#   TimingArc.cc: TimingArcSet full_name, sdf_cond, role, from, to,
#     timing_arcs iterator, from_edge_name, to_edge_name,
#     TimingArc from/to/fromEdge/toEdge/role,
#     timing_role_is_check
#   TableModel.cc: table model axis queries, value queries
#   Liberty.cc: findLibertyCell, findLibertyPort, timingArcSets iterator,
#     find_liberty_cells_matching regexp/nocase,
#     find_liberty_ports_matching, cell is_leaf/is_buffer/is_inverter,
#     liberty_library/test_cell/cell methods, port is_bus/is_bus_bit/
#     is_bundle/is_bundle_member/has_members/is_pwr_gnd/bus_name/
#     function/tristate_enable/scan_signal_type/set_direction,
#     LibertyPortMemberIterator, LibertyCellPortIterator
#   LibertyBuilder.cc: various cell build paths
source ../../test/helpers.tcl

############################################################
# Read library
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib

############################################################
# find_liberty_cells_matching with pattern/regexp/nocase
############################################################
set lib [lindex [get_libs NangateOpenCellLibrary] 0]

# Glob pattern matching
set cells [$lib find_liberty_cells_matching "INV_*" 0 0]

# Regexp matching
set cells_re [$lib find_liberty_cells_matching {^BUF_X[0-9]+$} 1 0]

# Case-insensitive matching
set cells_nc [$lib find_liberty_cells_matching "nand2_*" 0 1]

############################################################
# Cell property queries: is_leaf, is_buffer, is_inverter
############################################################
set inv_cell [get_lib_cell NangateOpenCellLibrary/INV_X1]
puts "INV_X1 is_leaf = [$inv_cell is_leaf]"
puts "INV_X1 is_buffer = [$inv_cell is_buffer]"
puts "INV_X1 is_inverter = [$inv_cell is_inverter]"

set buf_cell [get_lib_cell NangateOpenCellLibrary/BUF_X1]
puts "BUF_X1 is_leaf = [$buf_cell is_leaf]"
puts "BUF_X1 is_buffer = [$buf_cell is_buffer]"
puts "BUF_X1 is_inverter = [$buf_cell is_inverter]"

set nand_cell [get_lib_cell NangateOpenCellLibrary/NAND2_X1]
puts "NAND2_X1 is_leaf = [$nand_cell is_leaf]"
puts "NAND2_X1 is_buffer = [$nand_cell is_buffer]"
puts "NAND2_X1 is_inverter = [$nand_cell is_inverter]"

set dff_cell [get_lib_cell NangateOpenCellLibrary/DFF_X1]
puts "DFF_X1 is_leaf = [$dff_cell is_leaf]"
puts "DFF_X1 is_buffer = [$dff_cell is_buffer]"
puts "DFF_X1 is_inverter = [$dff_cell is_inverter]"

############################################################
# Cell liberty_library method
############################################################
set cell_lib [$inv_cell liberty_library]
puts "INV_X1 lib name = [$cell_lib name]"

############################################################
# Cell test_cell (for scan cells)
############################################################
set sdff_cell [get_lib_cell NangateOpenCellLibrary/SDFF_X1]
set test_cell [$sdff_cell test_cell]
if {$test_cell ne ""} {
  puts "SDFF_X1 has test_cell"
} else {
  puts "SDFF_X1 test_cell is null"
}

############################################################
# Port queries: bus_name, function, tristate_enable, scan_signal_type
############################################################

# Function queries
set inv_a [$inv_cell find_liberty_port A]
set inv_zn [$inv_cell find_liberty_port ZN]
puts "INV_X1/A function = [$inv_a function]"
puts "INV_X1/ZN function = [$inv_zn function]"

# Tristate enable
set tinv_cell [get_lib_cell NangateOpenCellLibrary/TINV_X1]
set tinv_en [$tinv_cell find_liberty_port EN]
set tinv_out [$tinv_cell find_liberty_port ZN]
puts "TINV_X1/EN function = [$tinv_en function]"
puts "TINV_X1/ZN tristate_enable = [$tinv_out tristate_enable]"

# Bus name (for bus ports - may be same as name for non-bus)
puts "INV_X1/A bus_name = [$inv_a bus_name]"

# Is bus/bundle queries
puts "INV_X1/A is_bus = [$inv_a is_bus]"
puts "INV_X1/A is_bus_bit = [$inv_a is_bus_bit]"
puts "INV_X1/A is_bundle = [$inv_a is_bundle]"
puts "INV_X1/A is_bundle_member = [$inv_a is_bundle_member]"
puts "INV_X1/A has_members = [$inv_a has_members]"

# is_pwr_gnd
puts "INV_X1/A is_pwr_gnd = [$inv_a is_pwr_gnd]"

# scan_signal_type
puts "INV_X1/A scan_signal_type = [$inv_a scan_signal_type]"

# Check SDFF scan port
set sdff_cell [get_lib_cell NangateOpenCellLibrary/SDFF_X1]
set sdff_si [$sdff_cell find_liberty_port SI]
if {$sdff_si ne ""} {
  puts "SDFF_X1/SI scan_signal_type = [$sdff_si scan_signal_type]"
  puts "SDFF_X1/SI is_bus = [$sdff_si is_bus]"
}

############################################################
# find_liberty_ports_matching on a cell
############################################################
set ports [$inv_cell find_liberty_ports_matching "*" 0 0]
puts "INV_X1 all ports = [llength $ports]"

set ports [$nand_cell find_liberty_ports_matching "A*" 0 0]
puts "NAND2_X1 A* ports = [llength $ports]"

# Regexp port matching
set ports_re [$nand_cell find_liberty_ports_matching {^A[0-9]$} 1 0]
puts "NAND2_X1 regexp ports = [llength $ports_re]"

# Case-insensitive port matching
set ports_nc [$nand_cell find_liberty_ports_matching "zn" 0 1]
puts "NAND2_X1 nocase zn ports = [llength $ports_nc]"

############################################################
# LibertyCellPortIterator
############################################################
set port_iter [$inv_cell liberty_port_iterator]
set port_count 0
while {[$port_iter has_next]} {
  set port [$port_iter next]
  incr port_count
}
$port_iter finish
puts "INV_X1 ports via iterator = $port_count"

# Port iterator on a more complex cell
set aoi_cell [get_lib_cell NangateOpenCellLibrary/AOI21_X1]
set port_iter [$aoi_cell liberty_port_iterator]
set port_count 0
while {[$port_iter has_next]} {
  set port [$port_iter next]
  set dir [sta::liberty_port_direction $port]
  incr port_count
}
$port_iter finish
puts "AOI21_X1 ports via iterator = $port_count"

############################################################
# Timing arc set queries: full_name, sdf_cond, role
############################################################
set arc_sets [$inv_cell timing_arc_sets]
foreach arc_set $arc_sets {
  set fn [$arc_set full_name]
  set from_port [$arc_set from]
  set to_port [$arc_set to]
  set role [$arc_set role]
  set is_check [sta::timing_role_is_check $role]
  puts "Arc: $fn role=$role is_check=$is_check"
  set sdf [$arc_set sdf_cond]
  puts "  sdf_cond=$sdf"
}

# DFF timing arcs (setup/hold/clk-to-q)
set arc_sets [$dff_cell timing_arc_sets]
foreach arc_set $arc_sets {
  set fn [$arc_set full_name]
  set role [$arc_set role]
  set is_check [sta::timing_role_is_check $role]
  puts "DFF Arc: $fn role=$role is_check=$is_check"
}

# DFFR has more arcs (recovery/removal)
set dffr_cell [get_lib_cell NangateOpenCellLibrary/DFFR_X1]
set arc_sets [$dffr_cell timing_arc_sets]
foreach arc_set $arc_sets {
  set fn [$arc_set full_name]
  set role [$arc_set role]
  set is_check [sta::timing_role_is_check $role]
  puts "DFFR Arc: $fn role=$role is_check=$is_check"
}

############################################################
# TimingArc details: from_edge_name, to_edge_name
############################################################
set arc_sets [$inv_cell timing_arc_sets]
foreach arc_set $arc_sets {
  set arcs [$arc_set timing_arcs]
  foreach arc $arcs {
    set from_name [[$arc from] bus_name]
    set to_name [[$arc to] bus_name]
    set from_edge [$arc from_edge_name]
    set to_edge [$arc to_edge_name]
    set arc_role [$arc role]
    puts "  Arc detail: ${from_name} ${from_edge} -> ${to_name} ${to_edge} role=$arc_role"
  }
}

# DFF arc details (different roles: setup, hold, clk-to-q)
set arc_sets [$dff_cell timing_arc_sets]
foreach arc_set $arc_sets {
  set arcs [$arc_set timing_arcs]
  foreach arc $arcs {
    set from_edge [$arc from_edge_name]
    set to_edge [$arc to_edge_name]
    set arc_role [$arc role]
    puts "  DFF arc: ${from_edge} -> ${to_edge} role=$arc_role"
  }
}

############################################################
# Operating conditions queries
############################################################
set op_cond [$lib default_operating_conditions]
if {$op_cond ne ""} {
  puts "Default opcond process = [$op_cond process]"
  puts "Default opcond voltage = [$op_cond voltage]"
  puts "Default opcond temperature = [$op_cond temperature]"
}

# Named operating conditions
set typical_cond [$lib find_operating_conditions typical]
if {$typical_cond ne ""} {
  puts "Typical opcond process = [$typical_cond process]"
  puts "Typical opcond voltage = [$typical_cond voltage]"
  puts "Typical opcond temperature = [$typical_cond temperature]"
}

############################################################
# Wireload queries
############################################################
set wl [$lib find_wireload "5K_hvratio_1_1"]
if {$wl ne ""} {
  puts "Found wireload 5K_hvratio_1_1"
}

set wlsel [$lib find_wireload_selection "WiresloaSelection"]
if {$wlsel ne ""} {
  puts "Found wireload selection"
}

############################################################
# LibertyLibraryIterator
############################################################
set lib_iter [sta::liberty_library_iterator]
set lib_count 0
while {[$lib_iter has_next]} {
  set lib [$lib_iter next]
  puts "Library: [$lib name]"
  incr lib_count
}
$lib_iter finish

############################################################
# Port capacitance with corner/min_max
############################################################
set corner [lindex [sta::corners] 0]
set inv_a_port [$inv_cell find_liberty_port A]
set cap_max [$inv_a_port capacitance $corner "max"]
puts "INV_X1/A cap max = $cap_max"
set cap_min [$inv_a_port capacitance $corner "min"]
puts "INV_X1/A cap min = $cap_min"

############################################################
# Power ground port queries
############################################################
set port_iter [$inv_cell liberty_port_iterator]
while {[$port_iter has_next]} {
  set port [$port_iter next]
  if {[$port is_pwr_gnd]} {
    puts "  PwrGnd port: [$port bus_name] dir=[sta::liberty_port_direction $port]"
  }
}
$port_iter finish

# Check a cell with bus ports (FA_X1 has bus-like ports)
set fa_cell [get_lib_cell NangateOpenCellLibrary/FA_X1]
set port_iter [$fa_cell liberty_port_iterator]
while {[$port_iter has_next]} {
  set port [$port_iter next]
  set dir [sta::liberty_port_direction $port]
  puts "  FA_X1 port: [$port bus_name] dir=$dir is_bus=[$port is_bus]"
}
$port_iter finish
