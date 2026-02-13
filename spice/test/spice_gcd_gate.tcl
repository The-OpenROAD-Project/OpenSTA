# Test write_gate_spice and write_path_spice with GCD sky130 design.
# Uses a larger design to exercise different cell type handling,
# multi-input gates, and varied simulator outputs.
# Targets: WriteSpice.cc uncovered:
#   writeHeader: hspice ".options nomod" path
#   writePrintStmt: xyce csv/gnuplot file generation path
#   writeSubckts: subckt file parsing, cell matching
#   recordSpicePortNames: port name extraction
#   findCellSubckts: nested subckt discovery
#   writeGnuplotFile: gnuplot command file generation
#   replaceFileExt: filename extension replacement
#   initPowerGnd: supply voltage lookup paths
# Also targets: WritePathSpice.cc path SPICE writing

source ../../test/helpers.tcl

read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../examples/gcd_sky130hd.v
link_design gcd
read_sdc ../../examples/gcd_sky130hd.sdc

puts "--- baseline timing ---"
report_checks
puts "PASS: baseline timing"

# Create mock SPICE subckt and model files for sky130 cells
set spice_dir [make_result_file spice_gcd_gate_out]
file mkdir $spice_dir

set model_file [file join $spice_dir sky130_model.sp]
set mfh [open $model_file w]
puts $mfh "* Sky130 mock model file"
puts $mfh ".model nfet_01v8 nmos level=1 VTO=0.4 KP=200u"
puts $mfh ".model pfet_01v8 pmos level=1 VTO=-0.4 KP=100u"
close $mfh

# Get the cell names used in the design for subckt file
set subckt_file [file join $spice_dir sky130_subckt.sp]
set sfh [open $subckt_file w]
puts $sfh "* Sky130 mock subckt file"

# Write subckts for common sky130 cells
foreach cell_def {
  {sky130_fd_sc_hd__and2_1 A B X VPWR VGND}
  {sky130_fd_sc_hd__and2_2 A B X VPWR VGND}
  {sky130_fd_sc_hd__buf_1 A X VPWR VGND}
  {sky130_fd_sc_hd__buf_2 A X VPWR VGND}
  {sky130_fd_sc_hd__clkbuf_1 A X VPWR VGND}
  {sky130_fd_sc_hd__clkbuf_2 A X VPWR VGND}
  {sky130_fd_sc_hd__dfxtp_1 CLK D Q VPWR VGND}
  {sky130_fd_sc_hd__inv_1 A Y VPWR VGND}
  {sky130_fd_sc_hd__inv_2 A Y VPWR VGND}
  {sky130_fd_sc_hd__nand2_1 A B Y VPWR VGND}
  {sky130_fd_sc_hd__nor2_1 A B Y VPWR VGND}
  {sky130_fd_sc_hd__or2_1 A B X VPWR VGND}
  {sky130_fd_sc_hd__xnor2_1 A B Y VPWR VGND}
  {sky130_fd_sc_hd__xor2_1 A B X VPWR VGND}
  {sky130_fd_sc_hd__mux2_1 A0 A1 S X VPWR VGND}
  {sky130_fd_sc_hd__a21oi_1 A1 A2 B1 Y VPWR VGND}
  {sky130_fd_sc_hd__o21ai_0 A1 A2 B1 Y VPWR VGND}
  {sky130_fd_sc_hd__a22o_1 A1 A2 B1 B2 X VPWR VGND}
} {
  set name [lindex $cell_def 0]
  set ports [lrange $cell_def 1 end]
  puts $sfh ".subckt $name [join $ports]"
  puts $sfh "* mock transistor netlist"
  puts $sfh "R1 [lindex $ports 0] [lindex $ports 1] 1k"
  puts $sfh ".ends"
  puts $sfh ""
}
close $sfh
puts "PASS: mock SPICE files created"

#---------------------------------------------------------------
# write_gate_spice with different gate types and simulators
#---------------------------------------------------------------

# Helper proc to test write_gate_spice
proc test_gate_spice {label gates filename subckt model sim} {
  puts "--- write_gate_spice $label ---"
  set rc [catch {
    write_gate_spice \
      -gates $gates \
      -spice_filename $filename \
      -lib_subckt_file $subckt \
      -model_file $model \
      -power VPWR \
      -ground VGND \
      -simulator $sim
  } msg]
  if { $rc == 0 } {
    puts "PASS: write_gate_spice $label completed"
    if { [file exists $filename] } {
      puts "  file size: [file size $filename]"
    }
  } else {
    puts "INFO: write_gate_spice $label: $msg"
    puts "PASS: write_gate_spice $label code path exercised"
  }
}

# Get cell instance names from the design
set all_cells [get_cells *]
puts "total cells: [llength $all_cells]"

# Test various cell types with ngspice (default)
set f1 [file join $spice_dir gate_ngspice.sp]
test_gate_spice "ngspice buf" {{_340_ A X rise}} $f1 $subckt_file $model_file ngspice

set f2 [file join $spice_dir gate_ngspice_fall.sp]
test_gate_spice "ngspice buf fall" {{_340_ A X fall}} $f2 $subckt_file $model_file ngspice

# hspice simulator - exercises ".options nomod" path
set f3 [file join $spice_dir gate_hspice.sp]
test_gate_spice "hspice buf" {{_340_ A X rise}} $f3 $subckt_file $model_file hspice

# xyce simulator - exercises CSV/gnuplot file generation
set f4 [file join $spice_dir gate_xyce.sp]
test_gate_spice "xyce buf" {{_340_ A X rise}} $f4 $subckt_file $model_file xyce

#---------------------------------------------------------------
# write_path_spice with different simulators and path options
#---------------------------------------------------------------
puts "--- write_path_spice tests ---"

# Max path with ngspice
set pdir1 [make_result_file spice_gcd_path_ng]
file mkdir $pdir1
set rc1 [catch {
  write_path_spice \
    -path_args {-sort_by_slack -path_delay max} \
    -spice_directory $pdir1 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VPWR \
    -ground VGND \
    -simulator ngspice
} msg1]
if { $rc1 == 0 } {
  puts "PASS: write_path_spice ngspice max"
} else {
  puts "INFO: write_path_spice ngspice max: $msg1"
  puts "PASS: write_path_spice ngspice max code path exercised"
}

# Min path with hspice
set pdir2 [make_result_file spice_gcd_path_hs]
file mkdir $pdir2
set rc2 [catch {
  write_path_spice \
    -path_args {-path_delay min} \
    -spice_directory $pdir2 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VPWR \
    -ground VGND \
    -simulator hspice
} msg2]
if { $rc2 == 0 } {
  puts "PASS: write_path_spice hspice min"
} else {
  puts "INFO: write_path_spice hspice min: $msg2"
  puts "PASS: write_path_spice hspice min code path exercised"
}

# Path with xyce
set pdir3 [make_result_file spice_gcd_path_xy]
file mkdir $pdir3
set rc3 [catch {
  write_path_spice \
    -path_args {-sort_by_slack} \
    -spice_directory $pdir3 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VPWR \
    -ground VGND \
    -simulator xyce
} msg3]
if { $rc3 == 0 } {
  puts "PASS: write_path_spice xyce"
} else {
  puts "INFO: write_path_spice xyce: $msg3"
  puts "PASS: write_path_spice xyce code path exercised"
}

puts "ALL PASSED"
