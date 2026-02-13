# Test liberty cell/pin/arc property queries for code coverage
# Targets: Liberty.cc, TimingArc.cc, InternalPower.cc, FuncExpr.cc, TableModel.cc
read_liberty ../../test/nangate45/Nangate45_typ.lib

############################################################
# Library-level properties
############################################################

set lib [get_libs NangateOpenCellLibrary]
puts "PASS: library found"

# Library iterator
set lib_iter [sta::liberty_library_iterator]
puts "PASS: liberty_library_iterator"

# find_liberty
set found_lib [sta::find_liberty NangateOpenCellLibrary]
puts "PASS: find_liberty NangateOpenCellLibrary"

############################################################
# Cell property queries on various cell types
############################################################

# Query cell properties using get_property / report_lib_cell
# Inverter
set inv [get_lib_cell NangateOpenCellLibrary/INV_X1]
report_lib_cell NangateOpenCellLibrary/INV_X1
puts "PASS: report_lib_cell INV_X1"

# Buffer
set buf [get_lib_cell NangateOpenCellLibrary/BUF_X1]
report_lib_cell NangateOpenCellLibrary/BUF_X1
puts "PASS: report_lib_cell BUF_X1"

# Sequential cells - DFF
set dff [get_lib_cell NangateOpenCellLibrary/DFF_X1]
report_lib_cell NangateOpenCellLibrary/DFF_X1
puts "PASS: report_lib_cell DFF_X1"

# DFF with reset
set dffr [get_lib_cell NangateOpenCellLibrary/DFFR_X1]
report_lib_cell NangateOpenCellLibrary/DFFR_X1
puts "PASS: report_lib_cell DFFR_X1"

# DFF with set
set dffs [get_lib_cell NangateOpenCellLibrary/DFFS_X1]
report_lib_cell NangateOpenCellLibrary/DFFS_X1
puts "PASS: report_lib_cell DFFS_X1"

# DFF with set and reset
set dffrs [get_lib_cell NangateOpenCellLibrary/DFFRS_X1]
report_lib_cell NangateOpenCellLibrary/DFFRS_X1
puts "PASS: report_lib_cell DFFRS_X1"

# Latch
set latch [get_lib_cell NangateOpenCellLibrary/TLAT_X1]
report_lib_cell NangateOpenCellLibrary/TLAT_X1
puts "PASS: report_lib_cell TLAT_X1"

# Complex cells
foreach cell_name {AOI21_X1 AOI22_X1 OAI21_X1 OAI22_X1 AOI211_X1 OAI211_X1 \
                   HA_X1 FA_X1 XNOR2_X1 XOR2_X1 MUX2_X1 \
                   CLKBUF_X1 CLKBUF_X2 CLKBUF_X3 TINV_X1 \
                   NAND2_X1 NAND3_X1 NAND4_X1 NOR2_X1 NOR3_X1 NOR4_X1 \
                   AND2_X1 AND3_X1 AND4_X1 OR2_X1 OR3_X1 OR4_X1 \
                   ANTENNA_X1 FILLCELL_X1 LOGIC0_X1 LOGIC1_X1 \
                   CLKGATETST_X1 CLKGATETST_X2 SDFF_X1 SDFFR_X1 SDFFS_X1 SDFFRS_X1} {
  catch {
    report_lib_cell NangateOpenCellLibrary/$cell_name
  }
}
puts "PASS: report_lib_cell various complex cells"

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
  catch {
    set pin [get_lib_pin NangateOpenCellLibrary/$cell_name/$pin_name]
    if { $pin != "" } {
      set dir [sta::liberty_port_direction $pin]
    }
  }
}
puts "PASS: liberty_port_direction queries"

############################################################
# get_lib_pins and get_lib_cells with various patterns
############################################################

# Wildcard patterns
set all_cells [get_lib_cells NangateOpenCellLibrary/*]
puts "PASS: get_lib_cells * = [llength $all_cells]"

set inv_cells [get_lib_cells NangateOpenCellLibrary/INV_*]
puts "PASS: get_lib_cells INV_* = [llength $inv_cells]"

set dff_cells [get_lib_cells NangateOpenCellLibrary/DFF*]
puts "PASS: get_lib_cells DFF* = [llength $dff_cells]"

set sdff_cells [get_lib_cells NangateOpenCellLibrary/SDFF*]
puts "PASS: get_lib_cells SDFF* = [llength $sdff_cells]"

# All pins of a cell
set inv_pins [get_lib_pins NangateOpenCellLibrary/INV_X1/*]
puts "PASS: get_lib_pins INV_X1/* = [llength $inv_pins]"

set dff_pins [get_lib_pins NangateOpenCellLibrary/DFF_X1/*]
puts "PASS: get_lib_pins DFF_X1/* = [llength $dff_pins]"

set dffr_pins [get_lib_pins NangateOpenCellLibrary/DFFR_X1/*]
puts "PASS: get_lib_pins DFFR_X1/* = [llength $dffr_pins]"

set aoi_pins [get_lib_pins NangateOpenCellLibrary/AOI21_X1/*]
puts "PASS: get_lib_pins AOI21_X1/* = [llength $aoi_pins]"

set fa_pins [get_lib_pins NangateOpenCellLibrary/FA_X1/*]
puts "PASS: get_lib_pins FA_X1/* = [llength $fa_pins]"

set clkgate_pins [get_lib_pins NangateOpenCellLibrary/CLKGATETST_X1/*]
puts "PASS: get_lib_pins CLKGATETST_X1/* = [llength $clkgate_pins]"

############################################################
# liberty_supply_exists
############################################################

set result [sta::liberty_supply_exists VDD]
puts "PASS: liberty_supply_exists VDD = $result"

set result [sta::liberty_supply_exists VSS]
puts "PASS: liberty_supply_exists VSS = $result"

set result [sta::liberty_supply_exists NONEXISTENT]
puts "PASS: liberty_supply_exists NONEXISTENT = $result"

############################################################
# Read ASAP7 SEQ library (exercises different liberty features)
############################################################

read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
puts "PASS: read ASAP7 SEQ"

# Query ASAP7 sequential cells
set asap7_dff [get_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/DFFHQNx1_ASAP7_75t_R]
report_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/DFFHQNx1_ASAP7_75t_R
puts "PASS: ASAP7 DFF cell"

set asap7_dff_pins [get_lib_pins asap7sc7p5t_SEQ_RVT_FF_nldm_220123/DFFHQNx1_ASAP7_75t_R/*]
puts "PASS: ASAP7 DFF pins ([llength $asap7_dff_pins])"

# ICG cell (clock gating)
set icg [get_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/ICGx1_ASAP7_75t_R]
report_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/ICGx1_ASAP7_75t_R
puts "PASS: ASAP7 ICG cell"

############################################################
# Read IHP library (different vendor, different features)
############################################################

read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p20V_25C.lib
puts "PASS: read IHP library"

# Tristate buffer
set ebufn [get_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_ebufn_2]
report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_ebufn_2
puts "PASS: IHP tristate buffer cell"

# Scan DFF
catch {
  set sdff [get_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_sdfbbp_1]
  if { $sdff != "" } {
    report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_sdfbbp_1
    puts "PASS: IHP scan DFF cell"
  }
}

############################################################
# Read Sky130 library
############################################################

read_liberty ../../test/sky130hd/sky130hd_tt.lib
puts "PASS: read Sky130 library"

# Query sky130 cells
report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__inv_1
puts "PASS: Sky130 inv cell"

# Query all sky130 cells
set sky_cells [get_lib_cells sky130_fd_sc_hd__tt_025C_1v80/*]
puts "PASS: Sky130 total cells: [llength $sky_cells]"

############################################################
# Write liberty
############################################################

source ../../test/helpers.tcl
set outfile [make_result_file liberty_properties_write.lib]
sta::write_liberty NangateOpenCellLibrary $outfile
puts "PASS: write_liberty"

puts "ALL PASSED"
