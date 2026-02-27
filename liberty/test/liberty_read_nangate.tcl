# Test Nangate45 library reading and cell/pin queries for code coverage
read_liberty ../../test/nangate45/Nangate45_typ.lib

############################################################
# Library queries
############################################################

set lib [get_libs NangateOpenCellLibrary]
if { $lib == "" } {
  puts "FAIL: library not found"
  exit 1
}

############################################################
# Inverter cells
############################################################

set inv1 [get_lib_cells NangateOpenCellLibrary/INV_X1]
if { $inv1 == "" } {
  puts "FAIL: INV_X1 not found"
  exit 1
}

report_lib_cell NangateOpenCellLibrary/INV_X1

set inv_pins [get_lib_pins NangateOpenCellLibrary/INV_X1/*]
if { [llength $inv_pins] != 2 } {
  error "expected 2 pins on INV_X1, found [llength $inv_pins]"
}

# Other inverter sizes
foreach sz {X2 X4 X8 X16 X32} {
  set cell [get_lib_cells NangateOpenCellLibrary/INV_$sz]
  if { $cell == "" } {
    puts "FAIL: INV_$sz not found"
    exit 1
  }
}

############################################################
# Buffer cells
############################################################

set buf1 [get_lib_cells NangateOpenCellLibrary/BUF_X1]
if { $buf1 == "" } {
  puts "FAIL: BUF_X1 not found"
  exit 1
}

report_lib_cell NangateOpenCellLibrary/BUF_X1

set buf_pins [get_lib_pins NangateOpenCellLibrary/BUF_X1/*]
if { [llength $buf_pins] != 2 } {
  error "expected 2 pins on BUF_X1, found [llength $buf_pins]"
}

foreach sz {X2 X4 X8 X16 X32} {
  set cell [get_lib_cells NangateOpenCellLibrary/BUF_$sz]
  if { $cell == "" } {
    puts "FAIL: BUF_$sz not found"
    exit 1
  }
}

############################################################
# NAND cells
############################################################

set nand2 [get_lib_cells NangateOpenCellLibrary/NAND2_X1]
if { $nand2 == "" } {
  puts "FAIL: NAND2_X1 not found"
  exit 1
}

report_lib_cell NangateOpenCellLibrary/NAND2_X1

set nand_pins [get_lib_pins NangateOpenCellLibrary/NAND2_X1/*]
if { [llength $nand_pins] != 3 } {
  error "expected 3 pins on NAND2_X1, found [llength $nand_pins]"
}

foreach cell_name {NAND2_X2 NAND2_X4 NAND3_X1 NAND3_X2 NAND4_X1} {
  set cell [get_lib_cells NangateOpenCellLibrary/$cell_name]
  if { $cell == "" } {
    puts "FAIL: $cell_name not found"
    exit 1
  }
}

############################################################
# NOR cells
############################################################

set nor2 [get_lib_cells NangateOpenCellLibrary/NOR2_X1]
if { $nor2 == "" } {
  puts "FAIL: NOR2_X1 not found"
  exit 1
}

report_lib_cell NangateOpenCellLibrary/NOR2_X1

foreach cell_name {NOR2_X2 NOR2_X4 NOR3_X1 NOR4_X1} {
  set cell [get_lib_cells NangateOpenCellLibrary/$cell_name]
  if { $cell == "" } {
    puts "FAIL: $cell_name not found"
    exit 1
  }
}

############################################################
# AND cells
############################################################

set and2 [get_lib_cells NangateOpenCellLibrary/AND2_X1]
if { $and2 == "" } {
  puts "FAIL: AND2_X1 not found"
  exit 1
}

report_lib_cell NangateOpenCellLibrary/AND2_X1

foreach cell_name {AND2_X2 AND2_X4 AND3_X1 AND4_X1} {
  set cell [get_lib_cells NangateOpenCellLibrary/$cell_name]
  if { $cell == "" } {
    puts "FAIL: $cell_name not found"
    exit 1
  }
}

############################################################
# OR cells
############################################################

set or2 [get_lib_cells NangateOpenCellLibrary/OR2_X1]
if { $or2 == "" } {
  puts "FAIL: OR2_X1 not found"
  exit 1
}

report_lib_cell NangateOpenCellLibrary/OR2_X1

foreach cell_name {OR2_X2 OR2_X4 OR3_X1 OR4_X1} {
  set cell [get_lib_cells NangateOpenCellLibrary/$cell_name]
  if { $cell == "" } {
    puts "FAIL: $cell_name not found"
    exit 1
  }
}

############################################################
# MUX cells
############################################################

set mux [get_lib_cells NangateOpenCellLibrary/MUX2_X1]
if { $mux == "" } {
  puts "FAIL: MUX2_X1 not found"
  exit 1
}

report_lib_cell NangateOpenCellLibrary/MUX2_X1

############################################################
# DFF cells
############################################################

set dff [get_lib_cells NangateOpenCellLibrary/DFF_X1]
if { $dff == "" } {
  puts "FAIL: DFF_X1 not found"
  exit 1
}

report_lib_cell NangateOpenCellLibrary/DFF_X1

set dff_pins [get_lib_pins NangateOpenCellLibrary/DFF_X1/*]

# DFF with reset
set dffr [get_lib_cells NangateOpenCellLibrary/DFFR_X1]
if { $dffr == "" } {
  puts "FAIL: DFFR_X1 not found"
  exit 1
}

report_lib_cell NangateOpenCellLibrary/DFFR_X1

# DFF with set
set dffs [get_lib_cells NangateOpenCellLibrary/DFFS_X1]
if { $dffs == "" } {
  puts "FAIL: DFFS_X1 not found"
  exit 1
}

report_lib_cell NangateOpenCellLibrary/DFFS_X1

# DFF with reset and set
set dffrs [get_lib_cells NangateOpenCellLibrary/DFFRS_X1]
if { $dffrs == "" } {
  puts "FAIL: DFFRS_X1 not found"
  exit 1
}

report_lib_cell NangateOpenCellLibrary/DFFRS_X1

############################################################
# Latch (TLAT)
############################################################

set tlat [get_lib_cells NangateOpenCellLibrary/TLAT_X1]
if { $tlat == "" } {
  puts "FAIL: TLAT_X1 not found"
  exit 1
}

report_lib_cell NangateOpenCellLibrary/TLAT_X1

############################################################
# Complex cells: AOI, OAI, HA, FA
############################################################

report_lib_cell NangateOpenCellLibrary/AOI21_X1

report_lib_cell NangateOpenCellLibrary/OAI21_X1

report_lib_cell NangateOpenCellLibrary/HA_X1

report_lib_cell NangateOpenCellLibrary/FA_X1

############################################################
# get_lib_cells with pattern matching
############################################################

set all_inv [get_lib_cells NangateOpenCellLibrary/INV_*]
if { [llength $all_inv] < 6 } {
  error "expected multiple INV_* cells, found [llength $all_inv]"
}

set all_buf [get_lib_cells NangateOpenCellLibrary/BUF_*]
if { [llength $all_buf] < 6 } {
  error "expected multiple BUF_* cells, found [llength $all_buf]"
}

set all_dff [get_lib_cells NangateOpenCellLibrary/DFF*]
if { [llength $all_dff] < 4 } {
  error "expected multiple DFF* cells, found [llength $all_dff]"
}

set all_cells [get_lib_cells NangateOpenCellLibrary/*]
if { [llength $all_cells] < 100 } {
  error "expected full Nangate library cell list, found [llength $all_cells]"
}

############################################################
# get_lib_pins with patterns
############################################################

set all_inv_pins [get_lib_pins NangateOpenCellLibrary/INV_X1/*]
if { [llength $all_inv_pins] != 2 } {
  error "expected 2 INV_X1 pins from wildcard, found [llength $all_inv_pins]"
}

set all_dff_pins [get_lib_pins NangateOpenCellLibrary/DFF_X1/*]
if { [llength $all_dff_pins] < 4 } {
  error "expected DFF_X1 pins to include D/CK/Q(+variants), found [llength $all_dff_pins]"
}

set nand_a1 [get_lib_pins NangateOpenCellLibrary/NAND2_X1/A1]
if { [llength $nand_a1] != 1 } {
  error "expected single NAND2_X1/A1 pin match"
}

############################################################
# Clock buffer
############################################################

report_lib_cell NangateOpenCellLibrary/CLKBUF_X1

report_lib_cell NangateOpenCellLibrary/CLKBUF_X2
