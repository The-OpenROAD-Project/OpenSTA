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

read_liberty ../../test/nangate45/Nangate45_fast.lib

read_liberty ../../test/nangate45/Nangate45_slow.lib

############################################################
# Make equiv cells for typ library
############################################################

set typ_lib [lindex [get_libs NangateOpenCellLibrary] 0]
sta::make_equiv_cells $typ_lib

############################################################
# Find equiv cells in typ library (various cell families)
############################################################

# INV family
set inv_x1 [get_lib_cell NangateOpenCellLibrary/INV_X1]
set inv_equivs [sta::find_equiv_cells $inv_x1]
puts "INV equiv count: [llength $inv_equivs]"

# BUF family
set buf_x1 [get_lib_cell NangateOpenCellLibrary/BUF_X1]
set buf_equivs [sta::find_equiv_cells $buf_x1]
puts "BUF equiv count: [llength $buf_equivs]"

# NAND2 family
set nand2_x1 [get_lib_cell NangateOpenCellLibrary/NAND2_X1]
set nand2_equivs [sta::find_equiv_cells $nand2_x1]
puts "NAND2 equiv count: [llength $nand2_equivs]"

# NAND3 family
set nand3_x1 [get_lib_cell NangateOpenCellLibrary/NAND3_X1]
set nand3_equivs [sta::find_equiv_cells $nand3_x1]
puts "NAND3 equiv count: [llength $nand3_equivs]"

# NAND4 family
set nand4_x1 [get_lib_cell NangateOpenCellLibrary/NAND4_X1]
set nand4_equivs [sta::find_equiv_cells $nand4_x1]
puts "NAND4 equiv count: [llength $nand4_equivs]"

# NOR2 family
set nor2_x1 [get_lib_cell NangateOpenCellLibrary/NOR2_X1]
set nor2_equivs [sta::find_equiv_cells $nor2_x1]
puts "NOR2 equiv count: [llength $nor2_equivs]"

# NOR3 family
set nor3_x1 [get_lib_cell NangateOpenCellLibrary/NOR3_X1]
set nor3_equivs [sta::find_equiv_cells $nor3_x1]
puts "NOR3 equiv count: [llength $nor3_equivs]"

# NOR4 family
set nor4_x1 [get_lib_cell NangateOpenCellLibrary/NOR4_X1]
set nor4_equivs [sta::find_equiv_cells $nor4_x1]
puts "NOR4 equiv count: [llength $nor4_equivs]"

# AND2 family
set and2_x1 [get_lib_cell NangateOpenCellLibrary/AND2_X1]
set and2_equivs [sta::find_equiv_cells $and2_x1]
puts "AND2 equiv count: [llength $and2_equivs]"

# OR2 family
set or2_x1 [get_lib_cell NangateOpenCellLibrary/OR2_X1]
set or2_equivs [sta::find_equiv_cells $or2_x1]
puts "OR2 equiv count: [llength $or2_equivs]"

# AOI21 family
set aoi21_x1 [get_lib_cell NangateOpenCellLibrary/AOI21_X1]
set aoi21_equivs [sta::find_equiv_cells $aoi21_x1]
puts "AOI21 equiv count: [llength $aoi21_equivs]"

# OAI21 family
set oai21_x1 [get_lib_cell NangateOpenCellLibrary/OAI21_X1]
set oai21_equivs [sta::find_equiv_cells $oai21_x1]
puts "OAI21 equiv count: [llength $oai21_equivs]"

# DFF family
set dff_x1 [get_lib_cell NangateOpenCellLibrary/DFF_X1]
set dff_equivs [sta::find_equiv_cells $dff_x1]
puts "DFF equiv count: [llength $dff_equivs]"

# SDFF family
set sdff_x1 [get_lib_cell NangateOpenCellLibrary/SDFF_X1]
set sdff_equivs [sta::find_equiv_cells $sdff_x1]
puts "SDFF equiv count: [llength $sdff_equivs]"

# CLKBUF family
set clkbuf_x1 [get_lib_cell NangateOpenCellLibrary/CLKBUF_X1]
set clkbuf_equivs [sta::find_equiv_cells $clkbuf_x1]
puts "CLKBUF equiv count: [llength $clkbuf_equivs]"

# XOR2 family
set xor2_x1 [get_lib_cell NangateOpenCellLibrary/XOR2_X1]
set xor2_equivs [sta::find_equiv_cells $xor2_x1]
puts "XOR2 equiv count: [llength $xor2_equivs]"

############################################################
# Cross-library equiv_cells comparisons
############################################################

set fast_inv_x1 [get_lib_cell NangateOpenCellLibrary_fast/INV_X1]
set slow_inv_x1 [get_lib_cell NangateOpenCellLibrary_slow/INV_X1]

set result [sta::equiv_cells $inv_x1 $fast_inv_x1]
puts "INV_X1 typ equiv fast: $result"

set result [sta::equiv_cells $inv_x1 $slow_inv_x1]
puts "INV_X1 typ equiv slow: $result"

set result [sta::equiv_cells $fast_inv_x1 $slow_inv_x1]
puts "INV_X1 fast equiv slow: $result"

# Cross-library BUF
set fast_buf_x1 [get_lib_cell NangateOpenCellLibrary_fast/BUF_X1]
set result [sta::equiv_cells $buf_x1 $fast_buf_x1]
puts "BUF_X1 typ equiv fast: $result"

# Cross-library NAND2
set fast_nand2_x1 [get_lib_cell NangateOpenCellLibrary_fast/NAND2_X1]
set result [sta::equiv_cells $nand2_x1 $fast_nand2_x1]
puts "NAND2_X1 typ equiv fast: $result"

# Cross-library DFF
set fast_dff_x1 [get_lib_cell NangateOpenCellLibrary_fast/DFF_X1]
set result [sta::equiv_cells $dff_x1 $fast_dff_x1]
puts "DFF_X1 typ equiv fast: $result"

############################################################
# equiv_cell_ports cross-library
############################################################

set result [sta::equiv_cell_ports $inv_x1 $fast_inv_x1]
puts "equiv_cell_ports INV typ/fast: $result"

set result [sta::equiv_cell_ports $buf_x1 $fast_buf_x1]
puts "equiv_cell_ports BUF typ/fast: $result"

# Different function should NOT match
set result [sta::equiv_cell_ports $inv_x1 $buf_x1]
puts "equiv_cell_ports INV/BUF: $result"

set result [sta::equiv_cell_ports $nand2_x1 $nand3_x1]
puts "equiv_cell_ports NAND2/NAND3: $result"

############################################################
# equiv_cell_timing_arcs cross-library
############################################################

set result [sta::equiv_cell_timing_arcs $inv_x1 $fast_inv_x1]
puts "equiv_cell_timing_arcs INV typ/fast: $result"

set result [sta::equiv_cell_timing_arcs $buf_x1 $fast_buf_x1]
puts "equiv_cell_timing_arcs BUF typ/fast: $result"

set result [sta::equiv_cell_timing_arcs $inv_x1 $buf_x1]
puts "equiv_cell_timing_arcs INV/BUF: $result"

############################################################
# Find library buffers for each library
############################################################

set typ_buffers [sta::find_library_buffers $typ_lib]
puts "typ library buffers: [llength $typ_buffers]"

set fast_lib [lindex [get_libs NangateOpenCellLibrary_fast] 0]
set fast_buffers [sta::find_library_buffers $fast_lib]
puts "fast library buffers: [llength $fast_buffers]"

set slow_lib [lindex [get_libs NangateOpenCellLibrary_slow] 0]
set slow_buffers [sta::find_library_buffers $slow_lib]
puts "slow library buffers: [llength $slow_buffers]"

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
puts "INV_X1 equiv INV_X2: $result"

set result [sta::equiv_cells $inv_x1 $inv_x4]
puts "INV_X1 equiv INV_X4: $result"

set result [sta::equiv_cells $inv_x1 $inv_x8]
puts "INV_X1 equiv INV_X8: $result"

set result [sta::equiv_cells $inv_x1 $inv_x16]
puts "INV_X1 equiv INV_X16: $result"

set result [sta::equiv_cells $inv_x1 $inv_x32]
puts "INV_X1 equiv INV_X32: $result"

# Different family comparisons
set result [sta::equiv_cells $nand2_x1 $nor2_x1]
puts "NAND2 equiv NOR2: $result"

set result [sta::equiv_cells $and2_x1 $or2_x1]
puts "AND2 equiv OR2: $result"

set result [sta::equiv_cells $aoi21_x1 $oai21_x1]
puts "AOI21 equiv OAI21: $result"

set dffr_x1 [get_lib_cell NangateOpenCellLibrary/DFFR_X1]
set result [sta::equiv_cells $dff_x1 $dffr_x1]
puts "DFF equiv DFFR: $result"

set dffs_x1 [get_lib_cell NangateOpenCellLibrary/DFFS_X1]
set result [sta::equiv_cells $dff_x1 $dffs_x1]
puts "DFF equiv DFFS: $result"

set dffrs_x1 [get_lib_cell NangateOpenCellLibrary/DFFRS_X1]
set result [sta::equiv_cells $dffr_x1 $dffrs_x1]
puts "DFFR equiv DFFRS: $result"

############################################################
# Read LVT library and make equiv cells
############################################################

read_liberty ../../test/nangate45/Nangate45_lvt.lib

set lvt_lib [lindex [get_libs NangateOpenCellLibrary_lvt] 0]
sta::make_equiv_cells $lvt_lib

set lvt_inv_x1 [get_lib_cell NangateOpenCellLibrary_lvt/INV_X1_L]
set lvt_inv_equivs [sta::find_equiv_cells $lvt_inv_x1]
puts "LVT INV equiv count: [llength $lvt_inv_equivs]"

set lvt_buffers [sta::find_library_buffers $lvt_lib]
puts "LVT library buffers: [llength $lvt_buffers]"

# Cross library with LVT (different cell naming so not equiv)
set result [sta::equiv_cells $inv_x1 $lvt_inv_x1]
puts "INV_X1 equiv LVT INV_X1_L: $result"

set result [sta::equiv_cell_ports $inv_x1 $lvt_inv_x1]
puts "equiv_cell_ports INV/LVT_INV: $result"
