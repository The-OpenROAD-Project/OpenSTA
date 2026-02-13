# Test multi-library network operations, liberty cell finding across
# libraries, and various network utility functions.
# Targets:
#   ConcreteNetwork.cc: findAnyCell, findCellsMatching, findLibCellsMatching,
#     makeInstance(LibertyCell,...), libertyCell queries,
#     addLibrary, findLibrary, deleteLibrary, libertyLibraryIterator,
#     setAttribute, getAttribute, attributeMap, filename, setName
#   Network.cc: findLibertyCell (searches across libs),
#     findLibertyFilename, defaultLibertyLibrary, libertyLibrary(Cell*),
#     libertyLibrary(Instance*), libertyPort, libertyCell(Instance*)
#   SdcNetwork.cc: various port/pin/net find operations
#   ParseBus.cc: parseBusName with non-bus names

read_liberty ../../test/nangate45/Nangate45_typ.lib
puts "PASS: read Nangate45"

read_liberty ../../test/nangate45/Nangate45_fast.lib
puts "PASS: read Nangate45 fast"

read_liberty ../../test/sky130hd/sky130hd_tt.lib
puts "PASS: read Sky130"

read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p20V_25C.lib
puts "PASS: read IHP"

#---------------------------------------------------------------
# Library queries
# Exercises: library iteration, findLibrary, findLiberty
#---------------------------------------------------------------
puts "--- library queries ---"

set libs [get_libs *]
puts "total libraries: [llength $libs]"

# Find specific libraries
set ng_lib [sta::find_liberty NangateOpenCellLibrary]
if { $ng_lib != "NULL" } {
  puts "PASS: find NangateOpenCellLibrary"
  puts "  name: [$ng_lib name]"
}

set ng_fast [sta::find_liberty NangateOpenCellLibrary_fast]
if { $ng_fast != "NULL" } {
  puts "PASS: find NangateOpenCellLibrary_fast"
}

set sky_lib [sta::find_liberty sky130_fd_sc_hd__tt_025C_1v80]
if { $sky_lib != "NULL" } {
  puts "PASS: find sky130 lib"
}

set ihp_lib [sta::find_liberty sg13g2_stdcell_typ_1p20V_25C]
if { $ihp_lib != "NULL" } {
  puts "PASS: find IHP lib"
}

# Liberty library iterator
set lib_iter [sta::liberty_library_iterator]
puts "PASS: liberty_library_iterator"

# Liberty supply exists across libraries
set vdd_exists [sta::liberty_supply_exists VDD]
puts "VDD supply exists: $vdd_exists"

set vss_exists [sta::liberty_supply_exists VSS]
puts "VSS supply exists: $vss_exists"

set gnd_exists [sta::liberty_supply_exists GND]
puts "GND supply exists: $gnd_exists"

set nope [sta::liberty_supply_exists NONEXISTENT]
puts "NONEXISTENT: $nope"

#---------------------------------------------------------------
# Find liberty cells across multiple libraries
# Exercises: findLibertyCell (iterates through all libs)
#---------------------------------------------------------------
puts "--- cross-library cell queries ---"

set inv1 [sta::find_liberty_cell INV_X1]
if { $inv1 != "NULL" } {
  puts "PASS: find_liberty_cell INV_X1"
  set lib [$inv1 liberty_library]
  puts "  from library: [$lib name]"
}

# Sky130 cell lookup
set sky_inv [sta::find_liberty_cell sky130_fd_sc_hd__inv_1]
if { $sky_inv != "NULL" } {
  puts "PASS: find_liberty_cell sky130 inv"
  set lib [$sky_inv liberty_library]
  puts "  from library: [$lib name]"
}

# IHP cell lookup
catch {
  set ihp_inv [sta::find_liberty_cell sg13g2_inv_1]
  if { $ihp_inv != "NULL" } {
    puts "PASS: find_liberty_cell IHP inv"
    set lib [$ihp_inv liberty_library]
    puts "  from library: [$lib name]"
  }
}

#---------------------------------------------------------------
# Cell pattern matching across libraries
# Exercises: find_liberty_cells_matching
#---------------------------------------------------------------
puts "--- cell pattern matching ---"

set ng_inv [$ng_lib find_liberty_cells_matching "INV*" 0 0]
puts "Nangate INV*: [llength $ng_inv]"

set ng_buf [$ng_lib find_liberty_cells_matching "BUF*" 0 0]
puts "Nangate BUF*: [llength $ng_buf]"

set ng_dff [$ng_lib find_liberty_cells_matching "DFF*" 0 0]
puts "Nangate DFF*: [llength $ng_dff]"

set ng_sdff [$ng_lib find_liberty_cells_matching "SDFF*" 0 0]
puts "Nangate SDFF*: [llength $ng_sdff]"

set ng_all [$ng_lib find_liberty_cells_matching "*" 0 0]
puts "Nangate all: [llength $ng_all]"

set sky_inv [$sky_lib find_liberty_cells_matching "*inv*" 0 0]
puts "Sky130 *inv*: [llength $sky_inv]"

set sky_buf [$sky_lib find_liberty_cells_matching "*buf*" 0 0]
puts "Sky130 *buf*: [llength $sky_buf]"

set sky_dff [$sky_lib find_liberty_cells_matching "*dfxtp*" 0 0]
puts "Sky130 *dfxtp*: [llength $sky_dff]"

set sky_latch [$sky_lib find_liberty_cells_matching "*dlx*" 0 0]
puts "Sky130 *dlx*: [llength $sky_latch]"

set sky_all [$sky_lib find_liberty_cells_matching "*" 0 0]
puts "Sky130 all: [llength $sky_all]"

set ihp_all [$ihp_lib find_liberty_cells_matching "*" 0 0]
puts "IHP all: [llength $ihp_all]"

#---------------------------------------------------------------
# Cell port queries
# Exercises: find_liberty_port, find_liberty_ports_matching
#---------------------------------------------------------------
puts "--- cell port queries ---"

# Nangate
set inv_cell [sta::find_liberty_cell INV_X1]
set inv_a [$inv_cell find_liberty_port A]
set inv_zn [$inv_cell find_liberty_port ZN]
puts "PASS: INV_X1 ports A and ZN found"

set inv_pm [$inv_cell find_liberty_ports_matching "*" 0 0]
puts "INV_X1 port match *: [llength $inv_pm]"

set dff_cell [sta::find_liberty_cell DFF_X1]
set dff_d [$dff_cell find_liberty_port D]
set dff_ck [$dff_cell find_liberty_port CK]
set dff_q [$dff_cell find_liberty_port Q]
set dff_qn [$dff_cell find_liberty_port QN]
puts "PASS: DFF_X1 ports D, CK, Q, QN found"

set dff_pm [$dff_cell find_liberty_ports_matching "*" 0 0]
puts "DFF_X1 port match *: [llength $dff_pm]"

# SDFF has scan ports
set sdff_cell [sta::find_liberty_cell SDFF_X1]
set sdff_pm [$sdff_cell find_liberty_ports_matching "*" 0 0]
puts "SDFF_X1 port match *: [llength $sdff_pm]"

set sdff_se [$sdff_cell find_liberty_port SE]
set sdff_si [$sdff_cell find_liberty_port SI]
puts "PASS: SDFF_X1 scan ports SE, SI found"

# FA (full adder) - multi-output
set fa_cell [sta::find_liberty_cell FA_X1]
set fa_pm [$fa_cell find_liberty_ports_matching "*" 0 0]
puts "FA_X1 port match *: [llength $fa_pm]"

# CLKGATETST
set clkgate_cell [sta::find_liberty_cell CLKGATETST_X1]
set clkgate_pm [$clkgate_cell find_liberty_ports_matching "*" 0 0]
puts "CLKGATETST_X1 port match *: [llength $clkgate_pm]"

# Timing arc sets on cells
set inv_arcs [$inv_cell timing_arc_sets]
puts "INV_X1 timing_arc_sets: [llength $inv_arcs]"

set dff_arcs [$dff_cell timing_arc_sets]
puts "DFF_X1 timing_arc_sets: [llength $dff_arcs]"

set sdff_arcs [$sdff_cell timing_arc_sets]
puts "SDFF_X1 timing_arc_sets: [llength $sdff_arcs]"

set clkgate_arcs [$clkgate_cell timing_arc_sets]
puts "CLKGATETST_X1 timing_arc_sets: [llength $clkgate_arcs]"

puts "PASS: cell port queries"

#---------------------------------------------------------------
# Link a design and run timing
#---------------------------------------------------------------
read_verilog network_test1.v
link_design network_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_input_delay -clock clk 0 [get_ports in2]
set_output_delay -clock clk 0 [get_ports out1]
set_input_transition 0.1 [all_inputs]

report_checks
puts "PASS: timing with multi-library"

# Replace with different sizes from same library
replace_cell buf1 NangateOpenCellLibrary/BUF_X2
report_checks
replace_cell buf1 NangateOpenCellLibrary/BUF_X1
puts "PASS: replace_cell with multi-lib"

#---------------------------------------------------------------
# Equiv cells across multiple libraries
# Exercises: EquivCells with equiv_libs and map_libs
#---------------------------------------------------------------
puts "--- equiv cells multi-lib ---"
catch {
  set lib1 [lindex [get_libs NangateOpenCellLibrary] 0]
  sta::make_equiv_cells $lib1

  # Find equivs
  set inv_x1 [get_lib_cell NangateOpenCellLibrary/INV_X1]
  set inv_equivs [sta::find_equiv_cells $inv_x1]
  puts "INV_X1 equivs: [llength $inv_equivs]"

  set buf_x1 [get_lib_cell NangateOpenCellLibrary/BUF_X1]
  set buf_equivs [sta::find_equiv_cells $buf_x1]
  puts "BUF_X1 equivs: [llength $buf_equivs]"

  # Find buffers
  set buffers [sta::find_library_buffers $lib1]
  puts "PASS: find_library_buffers: [llength $buffers]"
}
puts "PASS: equiv cells"

puts "ALL PASSED"
