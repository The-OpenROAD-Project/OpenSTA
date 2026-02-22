# Test SPICE writing with the GCD sky130hd design (larger design with many cell types).
source ../../test/helpers.tcl

read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../examples/gcd_sky130hd.v
link_design gcd
read_sdc ../../examples/gcd_sky130hd.sdc
read_spef ../../examples/gcd_sky130hd.spef

# Run timing
report_checks

report_checks -path_delay min

# Create SPICE model and subckt files covering all cell types used in GCD
set spice_dir [make_result_file spice_gcd]
file mkdir $spice_dir

set model_file [file join $spice_dir model.sp]
set model_fh [open $model_file w]
puts $model_fh "* SPICE model file for sky130hd"
puts $model_fh ".model nmos nmos level=3 tox=9e-9 vth0=0.7"
puts $model_fh ".model pmos pmos level=3 tox=9e-9 vth0=-0.7"
close $model_fh

# Create subcircuit definitions for cells used in GCD.
# We need to cover the cell types actually instantiated.
set subckt_file [file join $spice_dir subckt.sp]
set subckt_fh [open $subckt_file w]
puts $subckt_fh "* Subcircuit definitions for sky130hd cells used in gcd"
puts $subckt_fh ""

# Get the unique cell names used in the design
set cell_names [list]
set all_insts [get_cells *]
foreach inst $all_insts {
  set cell_ref [get_property $inst ref_name]
  if { [lsearch -exact $cell_names $cell_ref] == -1 } {
    lappend cell_names $cell_ref
  }
}
puts "unique cells: [llength $cell_names]"

# Write generic subckts for each cell type
foreach cell_name $cell_names {
  set lib_pins [get_lib_pins */${cell_name}/*]
  if { [llength $lib_pins] == 0 } { continue }
  set ports [list]
  foreach lp $lib_pins {
    lappend ports [get_property $lp name]
  }
  if { [llength $ports] >= 2 } {
    puts $subckt_fh ".subckt $cell_name [join $ports " "] VPWR VGND"
    puts $subckt_fh "* placeholder transistors"
    puts $subckt_fh "M1 [lindex $ports 0] [lindex $ports end] VPWR VPWR pmos W=0.4u L=0.15u"
    puts $subckt_fh "M2 [lindex $ports 0] [lindex $ports end] VGND VGND nmos W=0.2u L=0.15u"
    puts $subckt_fh ".ends"
    puts $subckt_fh ""
  }
}
close $subckt_fh

#---------------------------------------------------------------
# write_path_spice with max path (default ngspice)
# Exercises: writePathSpice, writeHeader, writeStageInstances,
#   writeSubckts, writePrintStmt, writeMeasureStmts, findCellSubckts
#---------------------------------------------------------------
puts "--- write_path_spice max ngspice ---"
set dir1 [make_result_file spice_gcd_max]
file mkdir $dir1
write_path_spice \
  -path_args {-sort_by_slack} \
  -spice_directory $dir1 \
  -lib_subckt_file $subckt_file \
  -model_file $model_file \
  -power VPWR \
  -ground VGND

#---------------------------------------------------------------
# write_path_spice with min path
#---------------------------------------------------------------
puts "--- write_path_spice min ---"
set dir2 [make_result_file spice_gcd_min]
file mkdir $dir2
write_path_spice \
  -path_args {-path_delay min} \
  -spice_directory $dir2 \
  -lib_subckt_file $subckt_file \
  -model_file $model_file \
  -power VPWR \
  -ground VGND

#---------------------------------------------------------------
# write_path_spice with hspice
# Exercises: hspice-specific nomod option in writeHeader
#---------------------------------------------------------------
puts "--- write_path_spice hspice ---"
set dir3 [make_result_file spice_gcd_hspice]
file mkdir $dir3
write_path_spice \
  -path_args {-sort_by_slack} \
  -spice_directory $dir3 \
  -lib_subckt_file $subckt_file \
  -model_file $model_file \
  -power VPWR \
  -ground VGND \
  -simulator hspice

#---------------------------------------------------------------
# write_path_spice with xyce
# Exercises: xyce-specific csv/gnuplot file creation
#---------------------------------------------------------------
puts "--- write_path_spice xyce ---"
set dir4 [make_result_file spice_gcd_xyce]
file mkdir $dir4
write_path_spice \
  -path_args {-sort_by_slack} \
  -spice_directory $dir4 \
  -lib_subckt_file $subckt_file \
  -model_file $model_file \
  -power VPWR \
  -ground VGND \
  -simulator xyce

#---------------------------------------------------------------
# write_path_spice with specific from/to
# Use valid register-to-output path (req_msg[0] -> resp_msg[13])
#---------------------------------------------------------------
puts "--- write_path_spice specific endpoints ---"
set dir5 [make_result_file spice_gcd_specific]
file mkdir $dir5
write_path_spice \
  -path_args {-from req_msg[0]} \
  -spice_directory $dir5 \
  -lib_subckt_file $subckt_file \
  -model_file $model_file \
  -power VPWR \
  -ground VGND
