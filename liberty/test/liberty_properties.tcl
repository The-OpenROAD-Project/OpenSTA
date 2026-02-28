# Test liberty cell/pin/arc property queries for code coverage
# Targets: Liberty.cc, TimingArc.cc, InternalPower.cc, FuncExpr.cc, TableModel.cc
read_liberty ../../test/nangate45/Nangate45_typ.lib

############################################################
# Library-level properties
############################################################

set lib [get_libs NangateOpenCellLibrary]

# Library iterator
set lib_iter [sta::liberty_library_iterator]

# find_liberty
set found_lib [sta::find_liberty NangateOpenCellLibrary]

############################################################
# Cell property queries on various cell types
############################################################

# Query cell properties using get_property / report_lib_cell
# Inverter
set inv [get_lib_cell NangateOpenCellLibrary/INV_X1]
report_lib_cell NangateOpenCellLibrary/INV_X1

# Buffer
set buf [get_lib_cell NangateOpenCellLibrary/BUF_X1]
report_lib_cell NangateOpenCellLibrary/BUF_X1

# Sequential cells - DFF
set dff [get_lib_cell NangateOpenCellLibrary/DFF_X1]
report_lib_cell NangateOpenCellLibrary/DFF_X1

# DFF with reset
set dffr [get_lib_cell NangateOpenCellLibrary/DFFR_X1]
report_lib_cell NangateOpenCellLibrary/DFFR_X1

# DFF with set
set dffs [get_lib_cell NangateOpenCellLibrary/DFFS_X1]
report_lib_cell NangateOpenCellLibrary/DFFS_X1

# DFF with set and reset
set dffrs [get_lib_cell NangateOpenCellLibrary/DFFRS_X1]
report_lib_cell NangateOpenCellLibrary/DFFRS_X1

# Latch
set latch [get_lib_cell NangateOpenCellLibrary/TLAT_X1]
report_lib_cell NangateOpenCellLibrary/TLAT_X1

# Complex cells
foreach cell_name {AOI21_X1 AOI22_X1 OAI21_X1 OAI22_X1 AOI211_X1 OAI211_X1 \
                   HA_X1 FA_X1 XNOR2_X1 XOR2_X1 MUX2_X1 \
                   CLKBUF_X1 CLKBUF_X2 CLKBUF_X3 TINV_X1 \
                   NAND2_X1 NAND3_X1 NAND4_X1 NOR2_X1 NOR3_X1 NOR4_X1 \
                   AND2_X1 AND3_X1 AND4_X1 OR2_X1 OR3_X1 OR4_X1 \
                   ANTENNA_X1 FILLCELL_X1 LOGIC0_X1 LOGIC1_X1 \
                   CLKGATETST_X1 CLKGATETST_X2 SDFF_X1 SDFFR_X1 SDFFS_X1 SDFFRS_X1} {
  report_lib_cell NangateOpenCellLibrary/$cell_name
}

############################################################
# Pin direction queries
############################################################

# Test liberty_port_direction on various pin types
foreach {cell_name pin_name} {
  INV_X1 A INV_X1 ZN
  BUF_X1 A BUF_X1 Z
  DFF_X1 D DFF_X1 CK DFF_X1 Q DFF_X1 QN
  NAND2_X1 A1 NAND2_X1 A2 NAND2_X1 ZN
  DFFR_X1 RN
  DFFS_X1 SN
  CLKGATETST_X1 CK CLKGATETST_X1 E CLKGATETST_X1 SE CLKGATETST_X1 GCK
} {
  set pin [get_lib_pin NangateOpenCellLibrary/$cell_name/$pin_name]
  if { $pin != "" } {
    set dir [sta::liberty_port_direction $pin]
  }
}

############################################################
# get_lib_pins and get_lib_cells with various patterns
############################################################

# Wildcard patterns
set all_cells [get_lib_cells NangateOpenCellLibrary/*]

set inv_cells [get_lib_cells NangateOpenCellLibrary/INV_*]

set dff_cells [get_lib_cells NangateOpenCellLibrary/DFF*]

set sdff_cells [get_lib_cells NangateOpenCellLibrary/SDFF*]

# All pins of a cell
set inv_pins [get_lib_pins NangateOpenCellLibrary/INV_X1/*]

set dff_pins [get_lib_pins NangateOpenCellLibrary/DFF_X1/*]

set dffr_pins [get_lib_pins NangateOpenCellLibrary/DFFR_X1/*]

set aoi_pins [get_lib_pins NangateOpenCellLibrary/AOI21_X1/*]

set fa_pins [get_lib_pins NangateOpenCellLibrary/FA_X1/*]

set clkgate_pins [get_lib_pins NangateOpenCellLibrary/CLKGATETST_X1/*]

############################################################
# liberty_supply_exists
############################################################

set result [sta::liberty_supply_exists VDD]

set result [sta::liberty_supply_exists VSS]

set result [sta::liberty_supply_exists NONEXISTENT]

############################################################
# Read ASAP7 SEQ library (exercises different liberty features)
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib

# Query ASAP7 sequential cells
set asap7_dff [get_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/DFFHQNx1_ASAP7_75t_R]
report_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/DFFHQNx1_ASAP7_75t_R

set asap7_dff_pins [get_lib_pins asap7sc7p5t_SEQ_RVT_FF_nldm_220123/DFFHQNx1_ASAP7_75t_R/*]

# ICG cell (clock gating)
set icg [get_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/ICGx1_ASAP7_75t_R]
report_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/ICGx1_ASAP7_75t_R

############################################################
# Read IHP library (different vendor, different features)
############################################################

read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p20V_25C.lib

# Tristate buffer
set ebufn [get_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_ebufn_2]
report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_ebufn_2

# Scan DFF
set sdff [get_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_sdfbbp_1]
if { $sdff != "" } {
  report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_sdfbbp_1
}

############################################################
# Read Sky130 library
############################################################

read_liberty ../../test/sky130hd/sky130hd_tt.lib

# Query sky130 cells
report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__inv_1

# Query all sky130 cells
set sky_cells [get_lib_cells sky130_fd_sc_hd__tt_025C_1v80/*]

############################################################
# Write liberty
############################################################

source ../../test/helpers.tcl
set outfile [make_result_file liberty_properties_write.lib]
sta::write_liberty NangateOpenCellLibrary $outfile
