# Test EquivCells pairwise comparisons across multiple PDKs.
# Exercises all comparison functions and drive-strength sorting.
# Targets:
#   EquivCells.cc: EquivCells constructor, findEquivCells,
#     hashCell, hashCellPorts, hashCellSequentials, hashSequential,
#     hashFuncExpr, hashPort, equivCells, equivCellPorts, equivCellFuncs,
#     equivCellSequentials, equivCellTimingArcSets, equivCellsArcs,
#     cellHasFuncs, cellDriveResistance, CellDriveResistanceGreater
source ../../test/helpers.tcl

############################################################
# Test 1: Nangate45 pairwise
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib

set ng_lib [lindex [get_libs NangateOpenCellLibrary] 0]
sta::make_equiv_cells $ng_lib

# Known-working families for find_equiv_cells
foreach cell_name {INV_X1 INV_X2 INV_X4 INV_X8 INV_X16 INV_X32
                   BUF_X1 BUF_X2 BUF_X4 BUF_X8 BUF_X16 BUF_X32
                   NAND2_X1 NAND2_X2 NAND2_X4
                   NOR2_X1 NOR2_X2 NOR2_X4
                   AND2_X1 AND2_X2 AND2_X4
                   OR2_X1 OR2_X2 OR2_X4
                   NAND3_X1 NAND3_X2 NAND3_X4
                   NOR3_X1 NOR3_X2 NOR3_X4
                   AND3_X1 AND3_X2 AND3_X4
                   OR3_X1 OR3_X2 OR3_X4
                   NAND4_X1 NAND4_X2 NAND4_X4
                   NOR4_X1 NOR4_X2 NOR4_X4
                   AND4_X1 AND4_X2 AND4_X4
                   OR4_X1 OR4_X2 OR4_X4
                   XOR2_X1 XOR2_X2
                   XNOR2_X1 XNOR2_X2
                   AOI21_X1 AOI21_X2 AOI21_X4
                   OAI21_X1 OAI21_X2 OAI21_X4
                   AOI22_X1 AOI22_X2 AOI22_X4
                   OAI22_X1 OAI22_X2 OAI22_X4
                   AOI211_X1 AOI211_X2
                   OAI211_X1 OAI211_X2 OAI211_X4
                   MUX2_X1 MUX2_X2
                   DFF_X1 DFF_X2
                   DFFR_X1 DFFR_X2
                   DFFS_X1 DFFS_X2
                   DFFRS_X1 DFFRS_X2
                   SDFF_X1 SDFF_X2
                   SDFFR_X1 SDFFR_X2
                   SDFFS_X1 SDFFS_X2
                   SDFFRS_X1 SDFFRS_X2
                   CLKBUF_X1 CLKBUF_X2 CLKBUF_X3
                   CLKGATETST_X1 CLKGATETST_X2 CLKGATETST_X4 CLKGATETST_X8} {
  set cell [get_lib_cell -quiet NangateOpenCellLibrary/$cell_name]
  if {$cell != ""} {
    set equivs [sta::find_equiv_cells $cell]
    if {$equivs != "" && $equivs != "NULL"} {
      puts "$cell_name equiv=[llength $equivs]"
    } else {
      puts "$cell_name equiv=0"
    }
  }
}

# Extensive pairwise comparisons
set inv_x1 [get_lib_cell NangateOpenCellLibrary/INV_X1]
set inv_x2 [get_lib_cell NangateOpenCellLibrary/INV_X2]
set buf_x1 [get_lib_cell NangateOpenCellLibrary/BUF_X1]
set buf_x2 [get_lib_cell NangateOpenCellLibrary/BUF_X2]
set nand2_x1 [get_lib_cell NangateOpenCellLibrary/NAND2_X1]
set nand2_x2 [get_lib_cell NangateOpenCellLibrary/NAND2_X2]
set nand3_x1 [get_lib_cell NangateOpenCellLibrary/NAND3_X1]
set nor2_x1 [get_lib_cell NangateOpenCellLibrary/NOR2_X1]
set nor2_x2 [get_lib_cell NangateOpenCellLibrary/NOR2_X2]
set and2_x1 [get_lib_cell NangateOpenCellLibrary/AND2_X1]
set or2_x1 [get_lib_cell NangateOpenCellLibrary/OR2_X1]
set xor2_x1 [get_lib_cell NangateOpenCellLibrary/XOR2_X1]
set dff_x1 [get_lib_cell NangateOpenCellLibrary/DFF_X1]
set dff_x2 [get_lib_cell NangateOpenCellLibrary/DFF_X2]
set dffr_x1 [get_lib_cell NangateOpenCellLibrary/DFFR_X1]
set sdff_x1 [get_lib_cell NangateOpenCellLibrary/SDFF_X1]
set clkgate_x1 [get_lib_cell NangateOpenCellLibrary/CLKGATETST_X1]
set clkgate_x2 [get_lib_cell NangateOpenCellLibrary/CLKGATETST_X2]

# Same-function equivalence (should be true)
puts "equiv INV_X1 INV_X2 = [sta::equiv_cells $inv_x1 $inv_x2]"
puts "equiv BUF_X1 BUF_X2 = [sta::equiv_cells $buf_x1 $buf_x2]"
puts "equiv NAND2_X1 NAND2_X2 = [sta::equiv_cells $nand2_x1 $nand2_x2]"
puts "equiv NOR2_X1 NOR2_X2 = [sta::equiv_cells $nor2_x1 $nor2_x2]"
puts "equiv DFF_X1 DFF_X2 = [sta::equiv_cells $dff_x1 $dff_x2]"
puts "equiv CLKGATE_X1 CLKGATE_X2 = [sta::equiv_cells $clkgate_x1 $clkgate_x2]"

# Different-function comparisons (should be false)
puts "equiv INV BUF = [sta::equiv_cells $inv_x1 $buf_x1]"
puts "equiv NAND2 NOR2 = [sta::equiv_cells $nand2_x1 $nor2_x1]"
puts "equiv NAND2 NAND3 = [sta::equiv_cells $nand2_x1 $nand3_x1]"
puts "equiv AND2 OR2 = [sta::equiv_cells $and2_x1 $or2_x1]"
puts "equiv DFF DFFR = [sta::equiv_cells $dff_x1 $dffr_x1]"
puts "equiv DFF SDFF = [sta::equiv_cells $dff_x1 $sdff_x1]"
puts "equiv INV DFF = [sta::equiv_cells $inv_x1 $dff_x1]"
puts "equiv INV CLKGATE = [sta::equiv_cells $inv_x1 $clkgate_x1]"

# Port equivalence
puts "ports INV_X1 INV_X2 = [sta::equiv_cell_ports $inv_x1 $inv_x2]"
puts "ports INV BUF = [sta::equiv_cell_ports $inv_x1 $buf_x1]"
puts "ports NAND2_X1 NAND2_X2 = [sta::equiv_cell_ports $nand2_x1 $nand2_x2]"
puts "ports NAND2 NAND3 = [sta::equiv_cell_ports $nand2_x1 $nand3_x1]"
puts "ports DFF_X1 DFF_X2 = [sta::equiv_cell_ports $dff_x1 $dff_x2]"
puts "ports DFF DFFR = [sta::equiv_cell_ports $dff_x1 $dffr_x1]"

# Timing arc equivalence
puts "arcs INV_X1 INV_X2 = [sta::equiv_cell_timing_arcs $inv_x1 $inv_x2]"
puts "arcs INV BUF = [sta::equiv_cell_timing_arcs $inv_x1 $buf_x1]"
puts "arcs DFF_X1 DFF_X2 = [sta::equiv_cell_timing_arcs $dff_x1 $dff_x2]"
puts "arcs DFF DFFR = [sta::equiv_cell_timing_arcs $dff_x1 $dffr_x1]"
puts "arcs NAND2_X1 NAND2_X2 = [sta::equiv_cell_timing_arcs $nand2_x1 $nand2_x2]"
puts "arcs NAND2 NOR2 = [sta::equiv_cell_timing_arcs $nand2_x1 $nor2_x1]"

set ng_bufs [sta::find_library_buffers $ng_lib]
puts "Nangate45 buffers = [llength $ng_bufs]"

############################################################
# Test 2: Sky130 families
############################################################
read_liberty ../../test/sky130hd/sky130hd_tt.lib

set sky_lib [lindex [get_libs sky130_fd_sc_hd__tt_025C_1v80] 0]
sta::make_equiv_cells $sky_lib

foreach cell_name {
  sky130_fd_sc_hd__inv_1 sky130_fd_sc_hd__inv_2 sky130_fd_sc_hd__inv_4 sky130_fd_sc_hd__inv_8
  sky130_fd_sc_hd__buf_1 sky130_fd_sc_hd__buf_2 sky130_fd_sc_hd__buf_4 sky130_fd_sc_hd__buf_8
  sky130_fd_sc_hd__nand2_1 sky130_fd_sc_hd__nand2_2 sky130_fd_sc_hd__nand2_4
  sky130_fd_sc_hd__nor2_1 sky130_fd_sc_hd__nor2_2 sky130_fd_sc_hd__nor2_4
  sky130_fd_sc_hd__and2_1 sky130_fd_sc_hd__and2_2 sky130_fd_sc_hd__and2_4
  sky130_fd_sc_hd__or2_1 sky130_fd_sc_hd__or2_2 sky130_fd_sc_hd__or2_4
  sky130_fd_sc_hd__dfxtp_1 sky130_fd_sc_hd__dfxtp_2 sky130_fd_sc_hd__dfxtp_4
  sky130_fd_sc_hd__dfrtp_1 sky130_fd_sc_hd__dfrtp_2 sky130_fd_sc_hd__dfrtp_4
  sky130_fd_sc_hd__clkbuf_1 sky130_fd_sc_hd__clkbuf_2 sky130_fd_sc_hd__clkbuf_4
} {
  set cell [get_lib_cell -quiet sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
  if {$cell != ""} {
    set equivs [sta::find_equiv_cells $cell]
    if {$equivs != "" && $equivs != "NULL"} {
      puts "SKY $cell_name equiv=[llength $equivs]"
    } else {
      puts "SKY $cell_name equiv=0"
    }
  }
}

set sky_bufs [sta::find_library_buffers $sky_lib]
puts "Sky130 buffers = [llength $sky_bufs]"

############################################################
# Test 3: IHP cell families
############################################################
read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p20V_25C.lib

set ihp_lib [lindex [get_libs sg13g2_stdcell_typ_1p20V_25C] 0]
sta::make_equiv_cells $ihp_lib

foreach cell_name {
  sg13g2_inv_1 sg13g2_inv_2 sg13g2_inv_4 sg13g2_inv_8
  sg13g2_buf_1 sg13g2_buf_2 sg13g2_buf_4 sg13g2_buf_8
  sg13g2_nand2_1 sg13g2_nand2_2
  sg13g2_nor2_1 sg13g2_nor2_2
  sg13g2_and2_1 sg13g2_and2_2
  sg13g2_or2_1 sg13g2_or2_2
} {
  set cell [get_lib_cell -quiet sg13g2_stdcell_typ_1p20V_25C/$cell_name]
  if {$cell != ""} {
    set equivs [sta::find_equiv_cells $cell]
    if {$equivs != "" && $equivs != "NULL"} {
      puts "IHP $cell_name equiv=[llength $equivs]"
    } else {
      puts "IHP $cell_name equiv=0"
    }
  }
}

set ihp_bufs [sta::find_library_buffers $ihp_lib]
puts "IHP buffers = [llength $ihp_bufs]"
