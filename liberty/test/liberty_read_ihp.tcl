# Test IHP SG13G2 library reading for code coverage
read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p20V_25C.lib

############################################################
# Library queries
############################################################

set lib [get_libs sg13g2_stdcell_typ_1p20V_25C]
if { $lib == "" } {
  puts "FAIL: IHP library not found"
  exit 1
}

############################################################
# Query various cell types
############################################################

# Inverters
set inv [get_lib_cells sg13g2_stdcell_typ_1p20V_25C/sg13g2_inv_1]
if { $inv == "" } {
  puts "FAIL: sg13g2_inv_1 not found"
  exit 1
}

report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_inv_1

set inv_pins [get_lib_pins sg13g2_stdcell_typ_1p20V_25C/sg13g2_inv_1/*]

foreach sz {2 4 8 16} {
  set cell [get_lib_cells sg13g2_stdcell_typ_1p20V_25C/sg13g2_inv_$sz]
  if { $cell == "" } {
    puts "FAIL: sg13g2_inv_$sz not found"
    exit 1
  }
}

# Buffers
set buf [get_lib_cells sg13g2_stdcell_typ_1p20V_25C/sg13g2_buf_1]
if { $buf == "" } {
  puts "FAIL: sg13g2_buf_1 not found"
  exit 1
}

report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_buf_1

# NAND gates
set nand2 [get_lib_cells sg13g2_stdcell_typ_1p20V_25C/sg13g2_nand2_1]
if { $nand2 == "" } {
  puts "FAIL: sg13g2_nand2_1 not found"
  exit 1
}

report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_nand2_1

foreach cell_name {sg13g2_nand2_2 sg13g2_nand3_1 sg13g2_nand4_1} {
  set cell [get_lib_cells sg13g2_stdcell_typ_1p20V_25C/$cell_name]
  if { $cell == "" } {
    puts "FAIL: $cell_name not found"
    exit 1
  }
}

# NOR gates
set nor2 [get_lib_cells sg13g2_stdcell_typ_1p20V_25C/sg13g2_nor2_1]
if { $nor2 == "" } {
  puts "FAIL: sg13g2_nor2_1 not found"
  exit 1
}

report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_nor2_1

foreach cell_name {sg13g2_nor2_2 sg13g2_nor3_1 sg13g2_nor4_1} {
  set cell [get_lib_cells sg13g2_stdcell_typ_1p20V_25C/$cell_name]
  if { $cell == "" } {
    puts "FAIL: $cell_name not found"
    exit 1
  }
}

# AND gates
set and2 [get_lib_cells sg13g2_stdcell_typ_1p20V_25C/sg13g2_and2_1]
if { $and2 == "" } {
  puts "FAIL: sg13g2_and2_1 not found"
  exit 1
}

report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_and2_1

foreach cell_name {sg13g2_and2_2 sg13g2_and3_1 sg13g2_and4_1} {
  set cell [get_lib_cells sg13g2_stdcell_typ_1p20V_25C/$cell_name]
  if { $cell == "" } {
    puts "FAIL: $cell_name not found"
    exit 1
  }
}

# MUX cells
set mux2 [get_lib_cells sg13g2_stdcell_typ_1p20V_25C/sg13g2_mux2_1]
if { $mux2 == "" } {
  puts "FAIL: sg13g2_mux2_1 not found"
  exit 1
}

report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_mux2_1

set mux4 [get_lib_cells sg13g2_stdcell_typ_1p20V_25C/sg13g2_mux4_1]
if { $mux4 == "" } {
  puts "FAIL: sg13g2_mux4_1 not found"
  exit 1
}

report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_mux4_1

# Flip-flop cells
set dfrbp [get_lib_cells sg13g2_stdcell_typ_1p20V_25C/sg13g2_dfrbp_1]
if { $dfrbp == "" } {
  puts "FAIL: sg13g2_dfrbp_1 not found"
  exit 1
}

report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_dfrbp_1

set dfrbp_pins [get_lib_pins sg13g2_stdcell_typ_1p20V_25C/sg13g2_dfrbp_1/*]

# Latch cells
set dlhq [get_lib_cells sg13g2_stdcell_typ_1p20V_25C/sg13g2_dlhq_1]
if { $dlhq == "" } {
  puts "FAIL: sg13g2_dlhq_1 not found"
  exit 1
}

report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_dlhq_1

# Complex cells (AOI)
set a21o [get_lib_cells sg13g2_stdcell_typ_1p20V_25C/sg13g2_a21o_1]
if { $a21o == "" } {
  puts "FAIL: sg13g2_a21o_1 not found"
  exit 1
}

report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_a21o_1

# XOR/XNOR
set xor [get_lib_cells sg13g2_stdcell_typ_1p20V_25C/sg13g2_xor2_1]
if { $xor == "" } {
  puts "FAIL: sg13g2_xor2_1 not found"
  exit 1
}

report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_xor2_1

set xnor [get_lib_cells sg13g2_stdcell_typ_1p20V_25C/sg13g2_xnor2_1]
if { $xnor == "" } {
  puts "FAIL: sg13g2_xnor2_1 not found"
  exit 1
}

report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_xnor2_1

# Tri-state / enable buffers
set ebufn [get_lib_cells sg13g2_stdcell_typ_1p20V_25C/sg13g2_ebufn_2]
if { $ebufn == "" } {
  puts "FAIL: sg13g2_ebufn_2 not found"
  exit 1
}

report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_ebufn_2

# Delay cells
set dlygate [get_lib_cells sg13g2_stdcell_typ_1p20V_25C/sg13g2_dlygate4sd1_1]
if { $dlygate == "" } {
  puts "FAIL: sg13g2_dlygate4sd1_1 not found"
  exit 1
}

report_lib_cell sg13g2_stdcell_typ_1p20V_25C/sg13g2_dlygate4sd1_1

############################################################
# Pattern matching across library
############################################################

set all_cells [get_lib_cells sg13g2_stdcell_typ_1p20V_25C/*]

set all_inv [get_lib_cells sg13g2_stdcell_typ_1p20V_25C/sg13g2_inv_*]

set all_nand [get_lib_cells sg13g2_stdcell_typ_1p20V_25C/sg13g2_nand*]

############################################################
# Also read the 1.50V variant
############################################################

read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p50V_25C.lib

set lib_1p5 [get_libs sg13g2_stdcell_typ_1p50V_25C]
if { $lib_1p5 == "" } {
  puts "FAIL: IHP 1p50V library not found"
  exit 1
}

set cells_1p5 [get_lib_cells sg13g2_stdcell_typ_1p50V_25C/*]
