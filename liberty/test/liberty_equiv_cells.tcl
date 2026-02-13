# Test equivalent cell finding for EquivCells.cc code coverage
read_liberty ../../test/nangate45/Nangate45_typ.lib

############################################################
# make_equiv_cells / find_equiv_cells / equiv_cells
############################################################

# Make equivalent cells for the Nangate library
set lib [lindex [get_libs NangateOpenCellLibrary] 0]
sta::make_equiv_cells $lib
puts "PASS: make_equiv_cells"

# Find equiv cells for various cell types
# INV_X1 should have equivalents (INV_X2, INV_X4, etc.)
set inv_cell [get_lib_cell NangateOpenCellLibrary/INV_X1]
set inv_equivs [sta::find_equiv_cells $inv_cell]
puts "PASS: find_equiv_cells INV_X1 ([llength $inv_equivs] equivs)"

set buf_cell [get_lib_cell NangateOpenCellLibrary/BUF_X1]
set buf_equivs [sta::find_equiv_cells $buf_cell]
puts "PASS: find_equiv_cells BUF_X1 ([llength $buf_equivs] equivs)"

set nand_cell [get_lib_cell NangateOpenCellLibrary/NAND2_X1]
set nand_equivs [sta::find_equiv_cells $nand_cell]
puts "PASS: find_equiv_cells NAND2_X1 ([llength $nand_equivs] equivs)"

set nor_cell [get_lib_cell NangateOpenCellLibrary/NOR2_X1]
set nor_equivs [sta::find_equiv_cells $nor_cell]
puts "PASS: find_equiv_cells NOR2_X1 ([llength $nor_equivs] equivs)"

set and_cell [get_lib_cell NangateOpenCellLibrary/AND2_X1]
set and_equivs [sta::find_equiv_cells $and_cell]
puts "PASS: find_equiv_cells AND2_X1 ([llength $and_equivs] equivs)"

set or_cell [get_lib_cell NangateOpenCellLibrary/OR2_X1]
set or_equivs [sta::find_equiv_cells $or_cell]
puts "PASS: find_equiv_cells OR2_X1 ([llength $or_equivs] equivs)"

# DFF cells
set dff_cell [get_lib_cell NangateOpenCellLibrary/DFF_X1]
set dff_equivs [sta::find_equiv_cells $dff_cell]
puts "PASS: find_equiv_cells DFF_X1 ([llength $dff_equivs] equivs)"

set dffr_cell [get_lib_cell NangateOpenCellLibrary/DFFR_X1]
set dffr_equivs [sta::find_equiv_cells $dffr_cell]
puts "PASS: find_equiv_cells DFFR_X1 ([llength $dffr_equivs] equivs)"

set dffs_cell [get_lib_cell NangateOpenCellLibrary/DFFS_X1]
set dffs_equivs [sta::find_equiv_cells $dffs_cell]
puts "PASS: find_equiv_cells DFFS_X1 ([llength $dffs_equivs] equivs)"

# AOI cells
set aoi_cell [get_lib_cell NangateOpenCellLibrary/AOI21_X1]
set aoi_equivs [sta::find_equiv_cells $aoi_cell]
puts "PASS: find_equiv_cells AOI21_X1 ([llength $aoi_equivs] equivs)"

# OAI cells
set oai_cell [get_lib_cell NangateOpenCellLibrary/OAI21_X1]
set oai_equivs [sta::find_equiv_cells $oai_cell]
puts "PASS: find_equiv_cells OAI21_X1 ([llength $oai_equivs] equivs)"

# MUX cells
set mux_cell [get_lib_cell NangateOpenCellLibrary/MUX2_X1]
set mux_equivs [sta::find_equiv_cells $mux_cell]
puts "PASS: find_equiv_cells MUX2_X1 ([llength $mux_equivs] equivs)"

# SDFF cells
set sdff_cell [get_lib_cell NangateOpenCellLibrary/SDFF_X1]
set sdff_equivs [sta::find_equiv_cells $sdff_cell]
puts "PASS: find_equiv_cells SDFF_X1 ([llength $sdff_equivs] equivs)"

############################################################
# equiv_cells comparison
############################################################

# Same-function cells should be equivalent
set inv_x1 [get_lib_cell NangateOpenCellLibrary/INV_X1]
set inv_x2 [get_lib_cell NangateOpenCellLibrary/INV_X2]
set result [sta::equiv_cells $inv_x1 $inv_x2]
puts "PASS: equiv_cells INV_X1 INV_X2 = $result"

set buf_x1 [get_lib_cell NangateOpenCellLibrary/BUF_X1]
set buf_x2 [get_lib_cell NangateOpenCellLibrary/BUF_X2]
set result [sta::equiv_cells $buf_x1 $buf_x2]
puts "PASS: equiv_cells BUF_X1 BUF_X2 = $result"

# Different-function cells should NOT be equivalent
set result [sta::equiv_cells $inv_x1 $buf_x1]
puts "PASS: equiv_cells INV_X1 BUF_X1 = $result"

set result [sta::equiv_cells $nand_cell $nor_cell]
puts "PASS: equiv_cells NAND2_X1 NOR2_X1 = $result"

# DFF equivalence
set dff_x1 [get_lib_cell NangateOpenCellLibrary/DFF_X1]
set dff_x2 [get_lib_cell NangateOpenCellLibrary/DFF_X2]
set result [sta::equiv_cells $dff_x1 $dff_x2]
puts "PASS: equiv_cells DFF_X1 DFF_X2 = $result"

# DFF vs DFFR (different function - has reset)
set result [sta::equiv_cells $dff_x1 $dffr_cell]
puts "PASS: equiv_cells DFF_X1 DFFR_X1 = $result"

# NAND2 vs NAND3 (different port count)
set nand3_cell [get_lib_cell NangateOpenCellLibrary/NAND3_X1]
set result [sta::equiv_cells $nand_cell $nand3_cell]
puts "PASS: equiv_cells NAND2_X1 NAND3_X1 = $result"

# Larger drive strengths
set inv_x4 [get_lib_cell NangateOpenCellLibrary/INV_X4]
set inv_x8 [get_lib_cell NangateOpenCellLibrary/INV_X8]
set result [sta::equiv_cells $inv_x4 $inv_x8]
puts "PASS: equiv_cells INV_X4 INV_X8 = $result"

############################################################
# equiv_cell_ports comparison
############################################################

set result [sta::equiv_cell_ports $inv_x1 $inv_x2]
puts "PASS: equiv_cell_ports INV_X1 INV_X2 = $result"

set result [sta::equiv_cell_ports $inv_x1 $buf_x1]
puts "PASS: equiv_cell_ports INV_X1 BUF_X1 = $result"

set nand2_x1 [get_lib_cell NangateOpenCellLibrary/NAND2_X1]
set nand2_x2 [get_lib_cell NangateOpenCellLibrary/NAND2_X2]
set result [sta::equiv_cell_ports $nand2_x1 $nand2_x2]
puts "PASS: equiv_cell_ports NAND2_X1 NAND2_X2 = $result"

# Different port count cells
set nand3_x1 [get_lib_cell NangateOpenCellLibrary/NAND3_X1]
set result [sta::equiv_cell_ports $nand2_x1 $nand3_x1]
puts "PASS: equiv_cell_ports NAND2_X1 NAND3_X1 = $result"

############################################################
# equiv_cell_timing_arcs comparison
############################################################

set result [sta::equiv_cell_timing_arcs $inv_x1 $inv_x2]
puts "PASS: equiv_cell_timing_arcs INV_X1 INV_X2 = $result"

set result [sta::equiv_cell_timing_arcs $buf_x1 $buf_x2]
puts "PASS: equiv_cell_timing_arcs BUF_X1 BUF_X2 = $result"

set result [sta::equiv_cell_timing_arcs $inv_x1 $buf_x1]
puts "PASS: equiv_cell_timing_arcs INV_X1 BUF_X1 = $result"

############################################################
# find_library_buffers
############################################################

set buffers [sta::find_library_buffers $lib]
puts "PASS: find_library_buffers ([llength $buffers] buffers)"

############################################################
# Additional library queries
############################################################

set found_lib [sta::find_liberty NangateOpenCellLibrary]
puts "PASS: find_liberty found"

set lib_iter [sta::liberty_library_iterator]
puts "PASS: liberty_library_iterator"

# liberty_supply_exists
set result [sta::liberty_supply_exists VDD]
puts "PASS: liberty_supply_exists VDD = $result"

set result [sta::liberty_supply_exists VSS]
puts "PASS: liberty_supply_exists VSS = $result"

set result [sta::liberty_supply_exists NONEXISTENT]
puts "PASS: liberty_supply_exists NONEXISTENT = $result"

# liberty_port_direction on various pins
set pin [get_lib_pin NangateOpenCellLibrary/INV_X1/A]
set dir [sta::liberty_port_direction $pin]
puts "PASS: INV_X1/A direction = $dir"

set pin [get_lib_pin NangateOpenCellLibrary/INV_X1/ZN]
set dir [sta::liberty_port_direction $pin]
puts "PASS: INV_X1/ZN direction = $dir"

set pin [get_lib_pin NangateOpenCellLibrary/DFF_X1/CK]
set dir [sta::liberty_port_direction $pin]
puts "PASS: DFF_X1/CK direction = $dir"

set pin [get_lib_pin NangateOpenCellLibrary/DFF_X1/Q]
set dir [sta::liberty_port_direction $pin]
puts "PASS: DFF_X1/Q direction = $dir"

############################################################
# EquivCells across fast library
############################################################

read_liberty ../../test/nangate45/Nangate45_fast.lib
set fast_lib [lindex [get_libs NangateOpenCellLibrary_fast] 0]
sta::make_equiv_cells $fast_lib
puts "PASS: make_equiv_cells fast library"

set fast_inv [get_lib_cell NangateOpenCellLibrary_fast/INV_X1]
set fast_inv_equivs [sta::find_equiv_cells $fast_inv]
puts "PASS: find_equiv_cells fast INV_X1 ([llength $fast_inv_equivs] equivs)"

# Cross-library equiv check
set result [sta::equiv_cells $inv_x1 $fast_inv]
puts "PASS: equiv_cells across libraries = $result"

puts "ALL PASSED"
