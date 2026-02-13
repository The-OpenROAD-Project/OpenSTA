# Deep equivalent cell testing for EquivCells.cc code coverage.
# Targets:
#   EquivCells.cc: findEquivCells, mapEquivCells,
#     hashCell, hashCellPorts, hashCellSequentials, hashSequential,
#     hashFuncExpr, hashPort, hashStatetable, hashStatetableRow,
#     equivCells, equivCellPorts, equivCellFuncs, equivCellSequentials,
#     equivCellStatetables, equivCellTimingArcSets, equivCellsArcs,
#     cellHasFuncs, cellDriveResistance, CellDriveResistanceGreater
#   Liberty.cc: Sequential related, Statetable related,
#     LibertyPort::equiv, FuncExpr::equiv
source ../../test/helpers.tcl

############################################################
# Read Nangate library
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib
puts "PASS: read Nangate45"

set lib [lindex [get_libs NangateOpenCellLibrary] 0]

############################################################
# Make equiv cells
############################################################
sta::make_equiv_cells $lib
puts "PASS: make_equiv_cells"

############################################################
# Test equiv cells for all major gate families
############################################################

# Inverters (should find INV_X1/X2/X4/X8/X16/X32 as equivalent)
set inv_x1 [get_lib_cell NangateOpenCellLibrary/INV_X1]
set equivs [sta::find_equiv_cells $inv_x1]
puts "INV_X1 equiv count = [llength $equivs]"
foreach eq $equivs {
  puts "  equiv: [$eq name]"
}
puts "PASS: INV equiv cells"

# Buffers
set buf_x1 [get_lib_cell NangateOpenCellLibrary/BUF_X1]
set equivs [sta::find_equiv_cells $buf_x1]
puts "BUF_X1 equiv count = [llength $equivs]"
foreach eq $equivs {
  puts "  equiv: [$eq name]"
}
puts "PASS: BUF equiv cells"

# 2-input gates
foreach gate {NAND2 NOR2 AND2 OR2 XOR2 XNOR2} {
  set cell [get_lib_cell NangateOpenCellLibrary/${gate}_X1]
  set equivs [sta::find_equiv_cells $cell]
  puts "${gate}_X1 equiv count = [llength $equivs]"
  puts "PASS: ${gate} equiv cells"
}

# 3-input gates
foreach gate {NAND3 NOR3 AND3 OR3} {
  catch {
    set cell [get_lib_cell NangateOpenCellLibrary/${gate}_X1]
    set equivs [sta::find_equiv_cells $cell]
    puts "${gate}_X1 equiv count = [llength $equivs]"
  }
}
puts "PASS: 3-input gate equiv cells"

# 4-input gates
foreach gate {NAND4 NOR4 AND4 OR4} {
  catch {
    set cell [get_lib_cell NangateOpenCellLibrary/${gate}_X1]
    set equivs [sta::find_equiv_cells $cell]
    puts "${gate}_X1 equiv count = [llength $equivs]"
  }
}
puts "PASS: 4-input gate equiv cells"

# AOI/OAI gates
foreach gate {AOI21 OAI21 AOI22 OAI22 AOI211 OAI211} {
  catch {
    set cell [get_lib_cell NangateOpenCellLibrary/${gate}_X1]
    set equivs [sta::find_equiv_cells $cell]
    puts "${gate}_X1 equiv count = [llength $equivs]"
  }
}
puts "PASS: AOI/OAI equiv cells"

# MUX cells
catch {
  set mux_cell [get_lib_cell NangateOpenCellLibrary/MUX2_X1]
  set equivs [sta::find_equiv_cells $mux_cell]
  puts "MUX2_X1 equiv count = [llength $equivs]"
}
puts "PASS: MUX equiv cells"

# DFF cells (sequential equivalence)
set dff_x1 [get_lib_cell NangateOpenCellLibrary/DFF_X1]
set dff_equivs [sta::find_equiv_cells $dff_x1]
puts "DFF_X1 equiv count = [llength $dff_equivs]"
foreach eq $dff_equivs {
  puts "  equiv: [$eq name]"
}
puts "PASS: DFF equiv cells"

# DFFR cells (reset flip-flop)
set dffr_x1 [get_lib_cell NangateOpenCellLibrary/DFFR_X1]
set dffr_equivs [sta::find_equiv_cells $dffr_x1]
puts "DFFR_X1 equiv count = [llength $dffr_equivs]"
puts "PASS: DFFR equiv cells"

# DFFS cells (set flip-flop)
set dffs_x1 [get_lib_cell NangateOpenCellLibrary/DFFS_X1]
set dffs_equivs [sta::find_equiv_cells $dffs_x1]
puts "DFFS_X1 equiv count = [llength $dffs_equivs]"
puts "PASS: DFFS equiv cells"

# SDFF cells (scan DFF)
set sdff_x1 [get_lib_cell NangateOpenCellLibrary/SDFF_X1]
set sdff_equivs [sta::find_equiv_cells $sdff_x1]
puts "SDFF_X1 equiv count = [llength $sdff_equivs]"
puts "PASS: SDFF equiv cells"

############################################################
# Cross-cell type equiv comparisons (should be false)
############################################################
set inv_x1 [get_lib_cell NangateOpenCellLibrary/INV_X1]
set buf_x1 [get_lib_cell NangateOpenCellLibrary/BUF_X1]
set nand2_x1 [get_lib_cell NangateOpenCellLibrary/NAND2_X1]
set nor2_x1 [get_lib_cell NangateOpenCellLibrary/NOR2_X1]
set dff_x1 [get_lib_cell NangateOpenCellLibrary/DFF_X1]

# Pairwise comparisons that should be false
puts "equiv INV BUF = [sta::equiv_cells $inv_x1 $buf_x1]"
puts "equiv INV NAND = [sta::equiv_cells $inv_x1 $nand2_x1]"
puts "equiv NAND NOR = [sta::equiv_cells $nand2_x1 $nor2_x1]"
puts "equiv INV DFF = [sta::equiv_cells $inv_x1 $dff_x1]"
puts "PASS: cross-type comparisons"

# Port equivalence detailed
puts "port_equiv INV_X1 INV_X2 = [sta::equiv_cell_ports $inv_x1 [get_lib_cell NangateOpenCellLibrary/INV_X2]]"
puts "port_equiv INV_X1 BUF_X1 = [sta::equiv_cell_ports $inv_x1 $buf_x1]"
puts "port_equiv NAND2_X1 NAND2_X2 = [sta::equiv_cell_ports $nand2_x1 [get_lib_cell NangateOpenCellLibrary/NAND2_X2]]"
puts "PASS: port equivalence"

# Timing arc equivalence
puts "arc_equiv INV_X1 INV_X2 = [sta::equiv_cell_timing_arcs $inv_x1 [get_lib_cell NangateOpenCellLibrary/INV_X2]]"
puts "arc_equiv INV_X1 BUF_X1 = [sta::equiv_cell_timing_arcs $inv_x1 $buf_x1]"
puts "arc_equiv DFF_X1 DFF_X2 = [sta::equiv_cell_timing_arcs $dff_x1 [get_lib_cell NangateOpenCellLibrary/DFF_X2]]"
puts "arc_equiv DFF_X1 DFFR_X1 = [sta::equiv_cell_timing_arcs $dff_x1 $dffr_x1]"
puts "PASS: timing arc equivalence"

############################################################
# Multi-library equivalence (exercises mapEquivCells)
############################################################
read_liberty ../../test/nangate45/Nangate45_fast.lib
puts "PASS: read Nangate45_fast"

set fast_lib [lindex [get_libs NangateOpenCellLibrary_fast] 0]
sta::make_equiv_cells $fast_lib
puts "PASS: make_equiv_cells fast lib"

# Cross-library comparisons
set fast_inv [get_lib_cell NangateOpenCellLibrary_fast/INV_X1]
set fast_buf [get_lib_cell NangateOpenCellLibrary_fast/BUF_X1]

puts "equiv typ_INV fast_INV = [sta::equiv_cells $inv_x1 $fast_inv]"
puts "equiv typ_INV fast_BUF = [sta::equiv_cells $inv_x1 $fast_buf]"
puts "port_equiv typ_INV fast_INV = [sta::equiv_cell_ports $inv_x1 $fast_inv]"
puts "arc_equiv typ_INV fast_INV = [sta::equiv_cell_timing_arcs $inv_x1 $fast_inv]"
puts "PASS: cross-library equivalence"

# Find equiv cells in the fast library
set fast_equivs [sta::find_equiv_cells $fast_inv]
puts "fast INV_X1 equiv count = [llength $fast_equivs]"
puts "PASS: fast lib equiv cells"

############################################################
# Find library buffers
############################################################
set lib [lindex [get_libs NangateOpenCellLibrary] 0]
set buffers [sta::find_library_buffers $lib]
puts "Library buffers count = [llength $buffers]"
foreach buf $buffers {
  puts "  buffer: [$buf name]"
}
puts "PASS: find_library_buffers"

puts "ALL PASSED"
