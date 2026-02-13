# Test findCellsMatching with various regex/glob patterns across multiple
# libraries, and exercise network property queries on matched cells.
# Targets:
#   Network.cc: findCellsMatching (glob and regex modes, nocase),
#     findCell, findPort, findNet, findPin,
#     isDriver, isLoad, portDirection, isInput, isOutput,
#     pathName, shortPathName, cellName, portName
#   ConcreteNetwork.cc: findCell, findCellsMatching,
#     portBitCount, portCount, cellNetworkView, setCellNetworkView
#   PatternMatch.cc: glob matching, regex matching, nocase matching
source ../../test/helpers.tcl

############################################################
# Read multiple libraries to have a rich cell database
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_liberty ../../test/sky130hd/sky130hd_tt.lib
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p20V_25C.lib
puts "PASS: read 6 libraries"

# Need a linked design for some operations
read_verilog ../../examples/example1.v
link_design top
puts "PASS: link design"

############################################################
# Glob pattern matching across all libraries
############################################################
puts "--- glob matching ---"

# Match all cells
set all [sta::find_cells_matching "*" 0 0]
puts "glob *: [llength $all] cells"

# Match specific prefixes
set inv_cells [sta::find_cells_matching "INV_*" 0 0]
puts "glob INV_*: [llength $inv_cells] cells"

set buf_cells [sta::find_cells_matching "BUF_*" 0 0]
puts "glob BUF_*: [llength $buf_cells] cells"

set dff_cells [sta::find_cells_matching "DFF*" 0 0]
puts "glob DFF*: [llength $dff_cells] cells"

set nand_cells [sta::find_cells_matching "*NAND*" 0 0]
puts "glob *NAND*: [llength $nand_cells] cells"

set nor_cells [sta::find_cells_matching "*NOR*" 0 0]
puts "glob *NOR*: [llength $nor_cells] cells"

# Sky130-specific patterns
set sky_inv [sta::find_cells_matching "sky130_fd_sc_hd__inv_*" 0 0]
puts "glob sky130 inv: [llength $sky_inv] cells"

set sky_buf [sta::find_cells_matching "sky130_fd_sc_hd__buf_*" 0 0]
puts "glob sky130 buf: [llength $sky_buf] cells"

set sky_dff [sta::find_cells_matching "sky130_fd_sc_hd__dfxtp_*" 0 0]
puts "glob sky130 dfxtp: [llength $sky_dff] cells"

# ASAP7-specific patterns
set asap7_inv [sta::find_cells_matching "INVx*_ASAP7_*" 0 0]
puts "glob ASAP7 INV: [llength $asap7_inv] cells"

set asap7_buf [sta::find_cells_matching "BUFx*_ASAP7_*" 0 0]
puts "glob ASAP7 BUF: [llength $asap7_buf] cells"

# IHP-specific patterns
set ihp_inv [sta::find_cells_matching "sg13g2_inv_*" 0 0]
puts "glob IHP inv: [llength $ihp_inv] cells"

set ihp_buf [sta::find_cells_matching "sg13g2_buf_*" 0 0]
puts "glob IHP buf: [llength $ihp_buf] cells"

# No-match pattern
set no_match [sta::find_cells_matching "NONEXISTENT_*" 0 0]
puts "glob NONEXISTENT: [llength $no_match] cells"

puts "PASS: glob matching"

############################################################
# Regex pattern matching
############################################################
puts "--- regex matching ---"

# Simple regex
set regex_inv [sta::find_cells_matching {^INV_X[0-9]+$} 1 0]
puts "regex INV_Xn: [llength $regex_inv] cells"

set regex_buf [sta::find_cells_matching {^BUF_X[0-9]+$} 1 0]
puts "regex BUF_Xn: [llength $regex_buf] cells"

set regex_nand [sta::find_cells_matching {^NAND[234]_X[0-9]+$} 1 0]
puts "regex NANDn: [llength $regex_nand] cells"

# More complex regex
set regex_all_inv [sta::find_cells_matching ".*inv.*" 1 0]
puts "regex .*inv.*: [llength $regex_all_inv] cells"

set regex_all_dff [sta::find_cells_matching {.*[Dd][Ff][Ff].*} 1 0]
puts "regex DFF: [llength $regex_all_dff] cells"

# Sky130 regex
set regex_sky_all [sta::find_cells_matching "^sky130_.*" 1 0]
puts "regex sky130: [llength $regex_sky_all] cells"

puts "PASS: regex matching"

############################################################
# Case-insensitive matching
############################################################
puts "--- nocase matching ---"

set nocase_inv [sta::find_cells_matching "inv_*" 0 1]
puts "nocase inv_*: [llength $nocase_inv] cells"

set nocase_buf [sta::find_cells_matching "buf_*" 0 1]
puts "nocase buf_*: [llength $nocase_buf] cells"

set nocase_dff [sta::find_cells_matching "dff*" 0 1]
puts "nocase dff*: [llength $nocase_dff] cells"

puts "PASS: nocase matching"

############################################################
# Liberty-level cell and port matching
############################################################
puts "--- liberty cell/port matching ---"
set ng_lib [lindex [get_libs NangateOpenCellLibrary] 0]

# Cell matching on specific library
set lib_inv [sta::find_cells_matching "INV_*" 0 0]
puts "lib INV: [llength $lib_inv]"

# Find specific liberty cell
set lc [sta::find_liberty_cell "NangateOpenCellLibrary/INV_X1"]
if {$lc != "NULL"} {
  puts "find_liberty_cell: [$lc name]"
}

# Find liberty from name
set found_lib [sta::find_liberty NangateOpenCellLibrary]
puts "find_liberty: [$found_lib name]"

set found_lib [sta::find_liberty sky130_fd_sc_hd__tt_025C_1v80]
puts "find_liberty: [$found_lib name]"

# Find liberty cell directly
set asap7_cell [sta::find_liberty_cell "asap7sc7p5t_INVBUF_RVT_FF_nldm_211120/INVx1_ASAP7_75t_R"]
if {$asap7_cell != "NULL"} {
  puts "ASAP7 find_liberty_cell: [$asap7_cell name]"
}

puts "PASS: liberty cell/port matching"

############################################################
# Design-level queries after linking
############################################################
puts "--- design queries ---"

# Find specific instances
foreach inst_name {r1 r2 r3 u1 u2} {
  set inst [get_cells $inst_name]
  set ref [get_property $inst ref_name]
  set fn [get_full_name $inst]
  puts "$inst_name: ref=$ref fn=$fn"
}
puts "PASS: instance queries"

# Find specific pins
foreach pin_path {r1/D r1/CK r1/Q u1/A u1/Z u2/A1 u2/A2 u2/ZN} {
  set pin [get_pins $pin_path]
  set dir [get_property $pin direction]
  set fn [get_full_name $pin]
  puts "$pin_path: dir=$dir fn=$fn"
}
puts "PASS: pin queries"

# Net driver/load queries
foreach net_name {r1q r2q u1z u2z} {
  set net [get_nets $net_name]
  set pins [get_pins -of_objects $net]
  puts "net $net_name: [llength $pins] pins"
}
puts "PASS: net queries"

############################################################
# Timing with the design
############################################################
puts "--- timing ---"
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}
set_output_delay -clock clk 0 [get_ports out]
set_input_transition 0.1 [all_inputs]

report_checks -endpoint_count 3
puts "PASS: timing"

report_checks -path_delay min
puts "PASS: min path"

report_checks -from [get_ports in1] -to [get_ports out]
puts "PASS: in1->out"

report_checks -from [get_ports in2] -to [get_ports out]
puts "PASS: in2->out"

puts "ALL PASSED"
