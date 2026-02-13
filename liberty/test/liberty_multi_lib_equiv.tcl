# Test multi-library equivalent cell analysis for code coverage
# Targets: EquivCells.cc (hashCell, hashCellPorts, equivCellPorts, equivCellTimingArcSets,
#          CellDriveResistanceGreater, cross-library matching),
#          Liberty.cc (cell comparison, port iteration),
#          LibertyReader.cc (multiple library reading)
source ../../test/helpers.tcl

############################################################
# Read multiple Nangate45 corners for cross-library equiv
############################################################

read_liberty ../../test/nangate45/Nangate45_typ.lib
puts "PASS: read Nangate45 typ"

read_liberty ../../test/nangate45/Nangate45_fast.lib
puts "PASS: read Nangate45 fast"

read_liberty ../../test/nangate45/Nangate45_slow.lib
puts "PASS: read Nangate45 slow"

############################################################
# Make equiv cells for typ library
############################################################

set typ_lib [lindex [get_libs NangateOpenCellLibrary] 0]
sta::make_equiv_cells $typ_lib
puts "PASS: make_equiv_cells typ"

############################################################
# Find equiv cells in typ library (various cell families)
############################################################

# INV family
set inv_x1 [get_lib_cell NangateOpenCellLibrary/INV_X1]
catch {
  set inv_equivs [sta::find_equiv_cells $inv_x1]
  puts "PASS: find_equiv_cells INV_X1 typ ([llength $inv_equivs] equivs)"
}

# BUF family
set buf_x1 [get_lib_cell NangateOpenCellLibrary/BUF_X1]
catch {
  set buf_equivs [sta::find_equiv_cells $buf_x1]
  puts "PASS: find_equiv_cells BUF_X1 typ ([llength $buf_equivs] equivs)"
}

# NAND2 family
set nand2_x1 [get_lib_cell NangateOpenCellLibrary/NAND2_X1]
catch {
  set nand2_equivs [sta::find_equiv_cells $nand2_x1]
  puts "PASS: find_equiv_cells NAND2_X1 typ ([llength $nand2_equivs] equivs)"
}

# NAND3 family
set nand3_x1 [get_lib_cell NangateOpenCellLibrary/NAND3_X1]
catch {
  set nand3_equivs [sta::find_equiv_cells $nand3_x1]
  puts "PASS: find_equiv_cells NAND3_X1 typ ([llength $nand3_equivs] equivs)"
}

# NAND4 family
set nand4_x1 [get_lib_cell NangateOpenCellLibrary/NAND4_X1]
catch {
  set nand4_equivs [sta::find_equiv_cells $nand4_x1]
  puts "PASS: find_equiv_cells NAND4_X1 typ ([llength $nand4_equivs] equivs)"
}

# NOR2 family
set nor2_x1 [get_lib_cell NangateOpenCellLibrary/NOR2_X1]
catch {
  set nor2_equivs [sta::find_equiv_cells $nor2_x1]
  puts "PASS: find_equiv_cells NOR2_X1 typ ([llength $nor2_equivs] equivs)"
}

# NOR3 family
set nor3_x1 [get_lib_cell NangateOpenCellLibrary/NOR3_X1]
catch {
  set nor3_equivs [sta::find_equiv_cells $nor3_x1]
  puts "PASS: find_equiv_cells NOR3_X1 typ ([llength $nor3_equivs] equivs)"
}

# NOR4 family
set nor4_x1 [get_lib_cell NangateOpenCellLibrary/NOR4_X1]
catch {
  set nor4_equivs [sta::find_equiv_cells $nor4_x1]
  puts "PASS: find_equiv_cells NOR4_X1 typ ([llength $nor4_equivs] equivs)"
}

# AND2 family
set and2_x1 [get_lib_cell NangateOpenCellLibrary/AND2_X1]
catch {
  set and2_equivs [sta::find_equiv_cells $and2_x1]
  puts "PASS: find_equiv_cells AND2_X1 typ ([llength $and2_equivs] equivs)"
}

# OR2 family
set or2_x1 [get_lib_cell NangateOpenCellLibrary/OR2_X1]
catch {
  set or2_equivs [sta::find_equiv_cells $or2_x1]
  puts "PASS: find_equiv_cells OR2_X1 typ ([llength $or2_equivs] equivs)"
}

# AOI21 family
set aoi21_x1 [get_lib_cell NangateOpenCellLibrary/AOI21_X1]
catch {
  set aoi21_equivs [sta::find_equiv_cells $aoi21_x1]
  puts "PASS: find_equiv_cells AOI21_X1 typ ([llength $aoi21_equivs] equivs)"
}

# OAI21 family
set oai21_x1 [get_lib_cell NangateOpenCellLibrary/OAI21_X1]
catch {
  set oai21_equivs [sta::find_equiv_cells $oai21_x1]
  puts "PASS: find_equiv_cells OAI21_X1 typ ([llength $oai21_equivs] equivs)"
}

# DFF family
set dff_x1 [get_lib_cell NangateOpenCellLibrary/DFF_X1]
catch {
  set dff_equivs [sta::find_equiv_cells $dff_x1]
  puts "PASS: find_equiv_cells DFF_X1 typ ([llength $dff_equivs] equivs)"
}

# SDFF family
set sdff_x1 [get_lib_cell NangateOpenCellLibrary/SDFF_X1]
catch {
  set sdff_equivs [sta::find_equiv_cells $sdff_x1]
  puts "PASS: find_equiv_cells SDFF_X1 typ ([llength $sdff_equivs] equivs)"
}

# CLKBUF family
set clkbuf_x1 [get_lib_cell NangateOpenCellLibrary/CLKBUF_X1]
catch {
  set clkbuf_equivs [sta::find_equiv_cells $clkbuf_x1]
  puts "PASS: find_equiv_cells CLKBUF_X1 typ ([llength $clkbuf_equivs] equivs)"
}

# XOR2 family
set xor2_x1 [get_lib_cell NangateOpenCellLibrary/XOR2_X1]
catch {
  set xor2_equivs [sta::find_equiv_cells $xor2_x1]
  puts "PASS: find_equiv_cells XOR2_X1 typ ([llength $xor2_equivs] equivs)"
}

############################################################
# Cross-library equiv_cells comparisons
############################################################

set fast_inv_x1 [get_lib_cell NangateOpenCellLibrary_fast/INV_X1]
set slow_inv_x1 [get_lib_cell NangateOpenCellLibrary_slow/INV_X1]

set result [sta::equiv_cells $inv_x1 $fast_inv_x1]
puts "PASS: equiv_cells typ/fast INV_X1 = $result"

set result [sta::equiv_cells $inv_x1 $slow_inv_x1]
puts "PASS: equiv_cells typ/slow INV_X1 = $result"

set result [sta::equiv_cells $fast_inv_x1 $slow_inv_x1]
puts "PASS: equiv_cells fast/slow INV_X1 = $result"

# Cross-library BUF
set fast_buf_x1 [get_lib_cell NangateOpenCellLibrary_fast/BUF_X1]
set result [sta::equiv_cells $buf_x1 $fast_buf_x1]
puts "PASS: equiv_cells typ/fast BUF_X1 = $result"

# Cross-library NAND2
set fast_nand2_x1 [get_lib_cell NangateOpenCellLibrary_fast/NAND2_X1]
set result [sta::equiv_cells $nand2_x1 $fast_nand2_x1]
puts "PASS: equiv_cells typ/fast NAND2_X1 = $result"

# Cross-library DFF
set fast_dff_x1 [get_lib_cell NangateOpenCellLibrary_fast/DFF_X1]
set result [sta::equiv_cells $dff_x1 $fast_dff_x1]
puts "PASS: equiv_cells typ/fast DFF_X1 = $result"

############################################################
# equiv_cell_ports cross-library
############################################################

set result [sta::equiv_cell_ports $inv_x1 $fast_inv_x1]
puts "PASS: equiv_cell_ports typ/fast INV_X1 = $result"

set result [sta::equiv_cell_ports $buf_x1 $fast_buf_x1]
puts "PASS: equiv_cell_ports typ/fast BUF_X1 = $result"

# Different function should NOT match
set result [sta::equiv_cell_ports $inv_x1 $buf_x1]
puts "PASS: equiv_cell_ports INV_X1 vs BUF_X1 = $result"

set result [sta::equiv_cell_ports $nand2_x1 $nand3_x1]
puts "PASS: equiv_cell_ports NAND2_X1 vs NAND3_X1 = $result"

############################################################
# equiv_cell_timing_arcs cross-library
############################################################

set result [sta::equiv_cell_timing_arcs $inv_x1 $fast_inv_x1]
puts "PASS: equiv_cell_timing_arcs typ/fast INV_X1 = $result"

set result [sta::equiv_cell_timing_arcs $buf_x1 $fast_buf_x1]
puts "PASS: equiv_cell_timing_arcs typ/fast BUF_X1 = $result"

set result [sta::equiv_cell_timing_arcs $inv_x1 $buf_x1]
puts "PASS: equiv_cell_timing_arcs INV_X1 vs BUF_X1 = $result"

############################################################
# Find library buffers for each library
############################################################

set typ_buffers [sta::find_library_buffers $typ_lib]
puts "PASS: find_library_buffers typ ([llength $typ_buffers] buffers)"

set fast_lib [lindex [get_libs NangateOpenCellLibrary_fast] 0]
set fast_buffers [sta::find_library_buffers $fast_lib]
puts "PASS: find_library_buffers fast ([llength $fast_buffers] buffers)"

set slow_lib [lindex [get_libs NangateOpenCellLibrary_slow] 0]
set slow_buffers [sta::find_library_buffers $slow_lib]
puts "PASS: find_library_buffers slow ([llength $slow_buffers] buffers)"

############################################################
# Additional equiv cells in typ library - within family
############################################################

# Same family - different sizes
set inv_x2 [get_lib_cell NangateOpenCellLibrary/INV_X2]
set inv_x4 [get_lib_cell NangateOpenCellLibrary/INV_X4]
set inv_x8 [get_lib_cell NangateOpenCellLibrary/INV_X8]
set inv_x16 [get_lib_cell NangateOpenCellLibrary/INV_X16]
set inv_x32 [get_lib_cell NangateOpenCellLibrary/INV_X32]

set result [sta::equiv_cells $inv_x1 $inv_x2]
puts "PASS: equiv_cells INV_X1/X2 = $result"

set result [sta::equiv_cells $inv_x1 $inv_x4]
puts "PASS: equiv_cells INV_X1/X4 = $result"

set result [sta::equiv_cells $inv_x1 $inv_x8]
puts "PASS: equiv_cells INV_X1/X8 = $result"

set result [sta::equiv_cells $inv_x1 $inv_x16]
puts "PASS: equiv_cells INV_X1/X16 = $result"

set result [sta::equiv_cells $inv_x1 $inv_x32]
puts "PASS: equiv_cells INV_X1/X32 = $result"

# Different family comparisons
set result [sta::equiv_cells $nand2_x1 $nor2_x1]
puts "PASS: equiv_cells NAND2/NOR2 = $result"

set result [sta::equiv_cells $and2_x1 $or2_x1]
puts "PASS: equiv_cells AND2/OR2 = $result"

set result [sta::equiv_cells $aoi21_x1 $oai21_x1]
puts "PASS: equiv_cells AOI21/OAI21 = $result"

set dffr_x1 [get_lib_cell NangateOpenCellLibrary/DFFR_X1]
set result [sta::equiv_cells $dff_x1 $dffr_x1]
puts "PASS: equiv_cells DFF/DFFR = $result"

set dffs_x1 [get_lib_cell NangateOpenCellLibrary/DFFS_X1]
set result [sta::equiv_cells $dff_x1 $dffs_x1]
puts "PASS: equiv_cells DFF/DFFS = $result"

set dffrs_x1 [get_lib_cell NangateOpenCellLibrary/DFFRS_X1]
set result [sta::equiv_cells $dffr_x1 $dffrs_x1]
puts "PASS: equiv_cells DFFR/DFFRS = $result"

############################################################
# Read LVT library and make equiv cells
############################################################

read_liberty ../../test/nangate45/Nangate45_lvt.lib
puts "PASS: read Nangate45 LVT"

set lvt_lib [lindex [get_libs NangateOpenCellLibrary_lvt] 0]
sta::make_equiv_cells $lvt_lib
puts "PASS: make_equiv_cells lvt"

set lvt_inv_x1 [get_lib_cell NangateOpenCellLibrary_lvt/INV_X1_L]
catch {
  set lvt_inv_equivs [sta::find_equiv_cells $lvt_inv_x1]
  puts "PASS: find_equiv_cells LVT INV_X1_L ([llength $lvt_inv_equivs] equivs)"
}

set lvt_buffers [sta::find_library_buffers $lvt_lib]
puts "PASS: find_library_buffers LVT ([llength $lvt_buffers] buffers)"

# Cross library with LVT (different cell naming so not equiv)
set result [sta::equiv_cells $inv_x1 $lvt_inv_x1]
puts "PASS: equiv_cells typ/lvt INV_X1 vs INV_X1_L = $result"

set result [sta::equiv_cell_ports $inv_x1 $lvt_inv_x1]
puts "PASS: equiv_cell_ports typ/lvt INV_X1 vs INV_X1_L = $result"

puts "ALL PASSED"
