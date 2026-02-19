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

# BUF family
set buf_x1 [get_lib_cell NangateOpenCellLibrary/BUF_X1]
set buf_equivs [sta::find_equiv_cells $buf_x1]

# NAND2 family
set nand2_x1 [get_lib_cell NangateOpenCellLibrary/NAND2_X1]
set nand2_equivs [sta::find_equiv_cells $nand2_x1]

# NAND3 family
set nand3_x1 [get_lib_cell NangateOpenCellLibrary/NAND3_X1]
set nand3_equivs [sta::find_equiv_cells $nand3_x1]

# NAND4 family
set nand4_x1 [get_lib_cell NangateOpenCellLibrary/NAND4_X1]
set nand4_equivs [sta::find_equiv_cells $nand4_x1]

# NOR2 family
set nor2_x1 [get_lib_cell NangateOpenCellLibrary/NOR2_X1]
set nor2_equivs [sta::find_equiv_cells $nor2_x1]

# NOR3 family
set nor3_x1 [get_lib_cell NangateOpenCellLibrary/NOR3_X1]
set nor3_equivs [sta::find_equiv_cells $nor3_x1]

# NOR4 family
set nor4_x1 [get_lib_cell NangateOpenCellLibrary/NOR4_X1]
set nor4_equivs [sta::find_equiv_cells $nor4_x1]

# AND2 family
set and2_x1 [get_lib_cell NangateOpenCellLibrary/AND2_X1]
set and2_equivs [sta::find_equiv_cells $and2_x1]

# OR2 family
set or2_x1 [get_lib_cell NangateOpenCellLibrary/OR2_X1]
set or2_equivs [sta::find_equiv_cells $or2_x1]

# AOI21 family
set aoi21_x1 [get_lib_cell NangateOpenCellLibrary/AOI21_X1]
set aoi21_equivs [sta::find_equiv_cells $aoi21_x1]

# OAI21 family
set oai21_x1 [get_lib_cell NangateOpenCellLibrary/OAI21_X1]
set oai21_equivs [sta::find_equiv_cells $oai21_x1]

# DFF family
set dff_x1 [get_lib_cell NangateOpenCellLibrary/DFF_X1]
set dff_equivs [sta::find_equiv_cells $dff_x1]

# SDFF family
set sdff_x1 [get_lib_cell NangateOpenCellLibrary/SDFF_X1]
set sdff_equivs [sta::find_equiv_cells $sdff_x1]

# CLKBUF family
set clkbuf_x1 [get_lib_cell NangateOpenCellLibrary/CLKBUF_X1]
set clkbuf_equivs [sta::find_equiv_cells $clkbuf_x1]

# XOR2 family
set xor2_x1 [get_lib_cell NangateOpenCellLibrary/XOR2_X1]
set xor2_equivs [sta::find_equiv_cells $xor2_x1]

############################################################
# Cross-library equiv_cells comparisons
############################################################

set fast_inv_x1 [get_lib_cell NangateOpenCellLibrary_fast/INV_X1]
set slow_inv_x1 [get_lib_cell NangateOpenCellLibrary_slow/INV_X1]

set result [sta::equiv_cells $inv_x1 $fast_inv_x1]

set result [sta::equiv_cells $inv_x1 $slow_inv_x1]

set result [sta::equiv_cells $fast_inv_x1 $slow_inv_x1]

# Cross-library BUF
set fast_buf_x1 [get_lib_cell NangateOpenCellLibrary_fast/BUF_X1]
set result [sta::equiv_cells $buf_x1 $fast_buf_x1]

# Cross-library NAND2
set fast_nand2_x1 [get_lib_cell NangateOpenCellLibrary_fast/NAND2_X1]
set result [sta::equiv_cells $nand2_x1 $fast_nand2_x1]

# Cross-library DFF
set fast_dff_x1 [get_lib_cell NangateOpenCellLibrary_fast/DFF_X1]
set result [sta::equiv_cells $dff_x1 $fast_dff_x1]

############################################################
# equiv_cell_ports cross-library
############################################################

set result [sta::equiv_cell_ports $inv_x1 $fast_inv_x1]

set result [sta::equiv_cell_ports $buf_x1 $fast_buf_x1]

# Different function should NOT match
set result [sta::equiv_cell_ports $inv_x1 $buf_x1]

set result [sta::equiv_cell_ports $nand2_x1 $nand3_x1]

############################################################
# equiv_cell_timing_arcs cross-library
############################################################

set result [sta::equiv_cell_timing_arcs $inv_x1 $fast_inv_x1]

set result [sta::equiv_cell_timing_arcs $buf_x1 $fast_buf_x1]

set result [sta::equiv_cell_timing_arcs $inv_x1 $buf_x1]

############################################################
# Find library buffers for each library
############################################################

set typ_buffers [sta::find_library_buffers $typ_lib]

set fast_lib [lindex [get_libs NangateOpenCellLibrary_fast] 0]
set fast_buffers [sta::find_library_buffers $fast_lib]

set slow_lib [lindex [get_libs NangateOpenCellLibrary_slow] 0]
set slow_buffers [sta::find_library_buffers $slow_lib]

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

set result [sta::equiv_cells $inv_x1 $inv_x4]

set result [sta::equiv_cells $inv_x1 $inv_x8]

set result [sta::equiv_cells $inv_x1 $inv_x16]

set result [sta::equiv_cells $inv_x1 $inv_x32]

# Different family comparisons
set result [sta::equiv_cells $nand2_x1 $nor2_x1]

set result [sta::equiv_cells $and2_x1 $or2_x1]

set result [sta::equiv_cells $aoi21_x1 $oai21_x1]

set dffr_x1 [get_lib_cell NangateOpenCellLibrary/DFFR_X1]
set result [sta::equiv_cells $dff_x1 $dffr_x1]

set dffs_x1 [get_lib_cell NangateOpenCellLibrary/DFFS_X1]
set result [sta::equiv_cells $dff_x1 $dffs_x1]

set dffrs_x1 [get_lib_cell NangateOpenCellLibrary/DFFRS_X1]
set result [sta::equiv_cells $dffr_x1 $dffrs_x1]

############################################################
# Read LVT library and make equiv cells
############################################################

read_liberty ../../test/nangate45/Nangate45_lvt.lib

set lvt_lib [lindex [get_libs NangateOpenCellLibrary_lvt] 0]
sta::make_equiv_cells $lvt_lib

set lvt_inv_x1 [get_lib_cell NangateOpenCellLibrary_lvt/INV_X1_L]
set lvt_inv_equivs [sta::find_equiv_cells $lvt_inv_x1]

set lvt_buffers [sta::find_library_buffers $lvt_lib]

# Cross library with LVT (different cell naming so not equiv)
set result [sta::equiv_cells $inv_x1 $lvt_inv_x1]

set result [sta::equiv_cell_ports $inv_x1 $lvt_inv_x1]
