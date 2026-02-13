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
puts "PASS: library loaded"

############################################################
# Inverter cells
############################################################

set inv1 [get_lib_cells NangateOpenCellLibrary/INV_X1]
if { $inv1 == "" } {
  puts "FAIL: INV_X1 not found"
  exit 1
}
puts "PASS: INV_X1 found"

report_lib_cell NangateOpenCellLibrary/INV_X1
puts "PASS: report_lib_cell INV_X1"

set inv_pins [get_lib_pins NangateOpenCellLibrary/INV_X1/*]
puts "PASS: INV_X1 pins ([llength $inv_pins] pins)"

# Other inverter sizes
foreach sz {X2 X4 X8 X16 X32} {
  set cell [get_lib_cells NangateOpenCellLibrary/INV_$sz]
  if { $cell == "" } {
    puts "FAIL: INV_$sz not found"
    exit 1
  }
}
puts "PASS: all INV sizes found"

############################################################
# Buffer cells
############################################################

set buf1 [get_lib_cells NangateOpenCellLibrary/BUF_X1]
if { $buf1 == "" } {
  puts "FAIL: BUF_X1 not found"
  exit 1
}
puts "PASS: BUF_X1 found"

report_lib_cell NangateOpenCellLibrary/BUF_X1
puts "PASS: report_lib_cell BUF_X1"

set buf_pins [get_lib_pins NangateOpenCellLibrary/BUF_X1/*]
puts "PASS: BUF_X1 pins ([llength $buf_pins] pins)"

foreach sz {X2 X4 X8 X16 X32} {
  set cell [get_lib_cells NangateOpenCellLibrary/BUF_$sz]
  if { $cell == "" } {
    puts "FAIL: BUF_$sz not found"
    exit 1
  }
}
puts "PASS: all BUF sizes found"

############################################################
# NAND cells
############################################################

set nand2 [get_lib_cells NangateOpenCellLibrary/NAND2_X1]
if { $nand2 == "" } {
  puts "FAIL: NAND2_X1 not found"
  exit 1
}
puts "PASS: NAND2_X1 found"

report_lib_cell NangateOpenCellLibrary/NAND2_X1
puts "PASS: report_lib_cell NAND2_X1"

set nand_pins [get_lib_pins NangateOpenCellLibrary/NAND2_X1/*]
puts "PASS: NAND2_X1 pins ([llength $nand_pins] pins)"

foreach cell_name {NAND2_X2 NAND2_X4 NAND3_X1 NAND3_X2 NAND4_X1} {
  set cell [get_lib_cells NangateOpenCellLibrary/$cell_name]
  if { $cell == "" } {
    puts "FAIL: $cell_name not found"
    exit 1
  }
}
puts "PASS: all NAND variants found"

############################################################
# NOR cells
############################################################

set nor2 [get_lib_cells NangateOpenCellLibrary/NOR2_X1]
if { $nor2 == "" } {
  puts "FAIL: NOR2_X1 not found"
  exit 1
}
puts "PASS: NOR2_X1 found"

report_lib_cell NangateOpenCellLibrary/NOR2_X1
puts "PASS: report_lib_cell NOR2_X1"

foreach cell_name {NOR2_X2 NOR2_X4 NOR3_X1 NOR4_X1} {
  set cell [get_lib_cells NangateOpenCellLibrary/$cell_name]
  if { $cell == "" } {
    puts "FAIL: $cell_name not found"
    exit 1
  }
}
puts "PASS: all NOR variants found"

############################################################
# AND cells
############################################################

set and2 [get_lib_cells NangateOpenCellLibrary/AND2_X1]
if { $and2 == "" } {
  puts "FAIL: AND2_X1 not found"
  exit 1
}
puts "PASS: AND2_X1 found"

report_lib_cell NangateOpenCellLibrary/AND2_X1
puts "PASS: report_lib_cell AND2_X1"

foreach cell_name {AND2_X2 AND2_X4 AND3_X1 AND4_X1} {
  set cell [get_lib_cells NangateOpenCellLibrary/$cell_name]
  if { $cell == "" } {
    puts "FAIL: $cell_name not found"
    exit 1
  }
}
puts "PASS: all AND variants found"

############################################################
# OR cells
############################################################

set or2 [get_lib_cells NangateOpenCellLibrary/OR2_X1]
if { $or2 == "" } {
  puts "FAIL: OR2_X1 not found"
  exit 1
}
puts "PASS: OR2_X1 found"

report_lib_cell NangateOpenCellLibrary/OR2_X1
puts "PASS: report_lib_cell OR2_X1"

foreach cell_name {OR2_X2 OR2_X4 OR3_X1 OR4_X1} {
  set cell [get_lib_cells NangateOpenCellLibrary/$cell_name]
  if { $cell == "" } {
    puts "FAIL: $cell_name not found"
    exit 1
  }
}
puts "PASS: all OR variants found"

############################################################
# MUX cells
############################################################

set mux [get_lib_cells NangateOpenCellLibrary/MUX2_X1]
if { $mux == "" } {
  puts "FAIL: MUX2_X1 not found"
  exit 1
}
puts "PASS: MUX2_X1 found"

report_lib_cell NangateOpenCellLibrary/MUX2_X1
puts "PASS: report_lib_cell MUX2_X1"

############################################################
# DFF cells
############################################################

set dff [get_lib_cells NangateOpenCellLibrary/DFF_X1]
if { $dff == "" } {
  puts "FAIL: DFF_X1 not found"
  exit 1
}
puts "PASS: DFF_X1 found"

report_lib_cell NangateOpenCellLibrary/DFF_X1
puts "PASS: report_lib_cell DFF_X1"

set dff_pins [get_lib_pins NangateOpenCellLibrary/DFF_X1/*]
puts "PASS: DFF_X1 pins ([llength $dff_pins] pins)"

# DFF with reset
set dffr [get_lib_cells NangateOpenCellLibrary/DFFR_X1]
if { $dffr == "" } {
  puts "FAIL: DFFR_X1 not found"
  exit 1
}
puts "PASS: DFFR_X1 found"

report_lib_cell NangateOpenCellLibrary/DFFR_X1
puts "PASS: report_lib_cell DFFR_X1"

# DFF with set
set dffs [get_lib_cells NangateOpenCellLibrary/DFFS_X1]
if { $dffs == "" } {
  puts "FAIL: DFFS_X1 not found"
  exit 1
}
puts "PASS: DFFS_X1 found"

report_lib_cell NangateOpenCellLibrary/DFFS_X1
puts "PASS: report_lib_cell DFFS_X1"

# DFF with reset and set
set dffrs [get_lib_cells NangateOpenCellLibrary/DFFRS_X1]
if { $dffrs == "" } {
  puts "FAIL: DFFRS_X1 not found"
  exit 1
}
puts "PASS: DFFRS_X1 found"

report_lib_cell NangateOpenCellLibrary/DFFRS_X1
puts "PASS: report_lib_cell DFFRS_X1"

############################################################
# Latch (TLAT)
############################################################

set tlat [get_lib_cells NangateOpenCellLibrary/TLAT_X1]
if { $tlat == "" } {
  puts "FAIL: TLAT_X1 not found"
  exit 1
}
puts "PASS: TLAT_X1 found"

report_lib_cell NangateOpenCellLibrary/TLAT_X1
puts "PASS: report_lib_cell TLAT_X1"

############################################################
# Complex cells: AOI, OAI, HA, FA
############################################################

report_lib_cell NangateOpenCellLibrary/AOI21_X1
puts "PASS: report_lib_cell AOI21_X1"

report_lib_cell NangateOpenCellLibrary/OAI21_X1
puts "PASS: report_lib_cell OAI21_X1"

report_lib_cell NangateOpenCellLibrary/HA_X1
puts "PASS: report_lib_cell HA_X1"

report_lib_cell NangateOpenCellLibrary/FA_X1
puts "PASS: report_lib_cell FA_X1"

############################################################
# get_lib_cells with pattern matching
############################################################

set all_inv [get_lib_cells NangateOpenCellLibrary/INV_*]
puts "PASS: get_lib_cells INV_* ([llength $all_inv] cells)"

set all_buf [get_lib_cells NangateOpenCellLibrary/BUF_*]
puts "PASS: get_lib_cells BUF_* ([llength $all_buf] cells)"

set all_dff [get_lib_cells NangateOpenCellLibrary/DFF*]
puts "PASS: get_lib_cells DFF* ([llength $all_dff] cells)"

set all_cells [get_lib_cells NangateOpenCellLibrary/*]
puts "PASS: get_lib_cells * ([llength $all_cells] total cells)"

############################################################
# get_lib_pins with patterns
############################################################

set all_inv_pins [get_lib_pins NangateOpenCellLibrary/INV_X1/*]
puts "PASS: get_lib_pins INV_X1/* ([llength $all_inv_pins] pins)"

set all_dff_pins [get_lib_pins NangateOpenCellLibrary/DFF_X1/*]
puts "PASS: get_lib_pins DFF_X1/* ([llength $all_dff_pins] pins)"

set nand_a1 [get_lib_pins NangateOpenCellLibrary/NAND2_X1/A1]
puts "PASS: get_lib_pins specific pin ([llength $nand_a1] pins)"

############################################################
# Clock buffer
############################################################

report_lib_cell NangateOpenCellLibrary/CLKBUF_X1
puts "PASS: report_lib_cell CLKBUF_X1"

report_lib_cell NangateOpenCellLibrary/CLKBUF_X2
puts "PASS: report_lib_cell CLKBUF_X2"

puts "ALL PASSED"
