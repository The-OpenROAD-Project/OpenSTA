# Test equivalent cell finding for EquivCells.cc code coverage
read_liberty ../../test/nangate45/Nangate45_typ.lib

############################################################
# make_equiv_cells / find_equiv_cells / equiv_cells
############################################################

# Make equivalent cells for the Nangate library
set lib [lindex [get_libs NangateOpenCellLibrary] 0]
sta::make_equiv_cells $lib

# Find equiv cells for various cell types
# INV_X1 should have equivalents (INV_X2, INV_X4, etc.)
set inv_cell [get_lib_cell NangateOpenCellLibrary/INV_X1]
set inv_equivs [sta::find_equiv_cells $inv_cell]

set buf_cell [get_lib_cell NangateOpenCellLibrary/BUF_X1]
set buf_equivs [sta::find_equiv_cells $buf_cell]

set nand_cell [get_lib_cell NangateOpenCellLibrary/NAND2_X1]
set nand_equivs [sta::find_equiv_cells $nand_cell]

set nor_cell [get_lib_cell NangateOpenCellLibrary/NOR2_X1]
set nor_equivs [sta::find_equiv_cells $nor_cell]

set and_cell [get_lib_cell NangateOpenCellLibrary/AND2_X1]
set and_equivs [sta::find_equiv_cells $and_cell]

set or_cell [get_lib_cell NangateOpenCellLibrary/OR2_X1]
set or_equivs [sta::find_equiv_cells $or_cell]

# DFF cells
set dff_cell [get_lib_cell NangateOpenCellLibrary/DFF_X1]
set dff_equivs [sta::find_equiv_cells $dff_cell]

set dffr_cell [get_lib_cell NangateOpenCellLibrary/DFFR_X1]
set dffr_equivs [sta::find_equiv_cells $dffr_cell]

set dffs_cell [get_lib_cell NangateOpenCellLibrary/DFFS_X1]
set dffs_equivs [sta::find_equiv_cells $dffs_cell]

# AOI cells
set aoi_cell [get_lib_cell NangateOpenCellLibrary/AOI21_X1]
set aoi_equivs [sta::find_equiv_cells $aoi_cell]

# OAI cells
set oai_cell [get_lib_cell NangateOpenCellLibrary/OAI21_X1]
set oai_equivs [sta::find_equiv_cells $oai_cell]

# MUX cells
set mux_cell [get_lib_cell NangateOpenCellLibrary/MUX2_X1]
set mux_equivs [sta::find_equiv_cells $mux_cell]

# SDFF cells
set sdff_cell [get_lib_cell NangateOpenCellLibrary/SDFF_X1]
set sdff_equivs [sta::find_equiv_cells $sdff_cell]

############################################################
# equiv_cells comparison
############################################################

# Same-function cells should be equivalent
set inv_x1 [get_lib_cell NangateOpenCellLibrary/INV_X1]
set inv_x2 [get_lib_cell NangateOpenCellLibrary/INV_X2]
set result [sta::equiv_cells $inv_x1 $inv_x2]

set buf_x1 [get_lib_cell NangateOpenCellLibrary/BUF_X1]
set buf_x2 [get_lib_cell NangateOpenCellLibrary/BUF_X2]
set result [sta::equiv_cells $buf_x1 $buf_x2]

# Different-function cells should NOT be equivalent
set result [sta::equiv_cells $inv_x1 $buf_x1]

set result [sta::equiv_cells $nand_cell $nor_cell]

# DFF equivalence
set dff_x1 [get_lib_cell NangateOpenCellLibrary/DFF_X1]
set dff_x2 [get_lib_cell NangateOpenCellLibrary/DFF_X2]
set result [sta::equiv_cells $dff_x1 $dff_x2]

# DFF vs DFFR (different function - has reset)
set result [sta::equiv_cells $dff_x1 $dffr_cell]

# NAND2 vs NAND3 (different port count)
set nand3_cell [get_lib_cell NangateOpenCellLibrary/NAND3_X1]
set result [sta::equiv_cells $nand_cell $nand3_cell]

# Larger drive strengths
set inv_x4 [get_lib_cell NangateOpenCellLibrary/INV_X4]
set inv_x8 [get_lib_cell NangateOpenCellLibrary/INV_X8]
set result [sta::equiv_cells $inv_x4 $inv_x8]

############################################################
# equiv_cell_ports comparison
############################################################

set result [sta::equiv_cell_ports $inv_x1 $inv_x2]

set result [sta::equiv_cell_ports $inv_x1 $buf_x1]

set nand2_x1 [get_lib_cell NangateOpenCellLibrary/NAND2_X1]
set nand2_x2 [get_lib_cell NangateOpenCellLibrary/NAND2_X2]
set result [sta::equiv_cell_ports $nand2_x1 $nand2_x2]

# Different port count cells
set nand3_x1 [get_lib_cell NangateOpenCellLibrary/NAND3_X1]
set result [sta::equiv_cell_ports $nand2_x1 $nand3_x1]

############################################################
# equiv_cell_timing_arcs comparison
############################################################

set result [sta::equiv_cell_timing_arcs $inv_x1 $inv_x2]

set result [sta::equiv_cell_timing_arcs $buf_x1 $buf_x2]

set result [sta::equiv_cell_timing_arcs $inv_x1 $buf_x1]

############################################################
# find_library_buffers
############################################################

set buffers [sta::find_library_buffers $lib]

############################################################
# Additional library queries
############################################################

set found_lib [sta::find_liberty NangateOpenCellLibrary]

set lib_iter [sta::liberty_library_iterator]

# liberty_supply_exists
set result [sta::liberty_supply_exists VDD]

set result [sta::liberty_supply_exists VSS]

set result [sta::liberty_supply_exists NONEXISTENT]

# liberty_port_direction on various pins
set pin [get_lib_pin NangateOpenCellLibrary/INV_X1/A]
set dir [sta::liberty_port_direction $pin]

set pin [get_lib_pin NangateOpenCellLibrary/INV_X1/ZN]
set dir [sta::liberty_port_direction $pin]

set pin [get_lib_pin NangateOpenCellLibrary/DFF_X1/CK]
set dir [sta::liberty_port_direction $pin]

set pin [get_lib_pin NangateOpenCellLibrary/DFF_X1/Q]
set dir [sta::liberty_port_direction $pin]

############################################################
# EquivCells across fast library
############################################################

read_liberty ../../test/nangate45/Nangate45_fast.lib
set fast_lib [lindex [get_libs NangateOpenCellLibrary_fast] 0]
sta::make_equiv_cells $fast_lib

set fast_inv [get_lib_cell NangateOpenCellLibrary_fast/INV_X1]
set fast_inv_equivs [sta::find_equiv_cells $fast_inv]

# Cross-library equiv check
set result [sta::equiv_cells $inv_x1 $fast_inv]
