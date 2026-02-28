# Test write_path_spice with GCD sky130 design.
# Uses a larger design to exercise different cell type handling,
# multi-input gates, and varied simulator outputs.
# NOTE: write_gate_spice tests removed - write_gate_spice_cmd SWIG binding
# is missing. See bug_report_missing_write_gate_spice_cmd.md.
source ../../test/helpers.tcl

read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../examples/gcd_sky130hd.v
link_design gcd
read_sdc ../../examples/gcd_sky130hd.sdc

puts "--- baseline timing ---"
report_checks

# Create mock SPICE subckt and model files for sky130 cells
set spice_dir [make_result_file spice_gcd_gate_out]
file mkdir $spice_dir

set model_file [file join $spice_dir sky130_model.sp]
set mfh [open $model_file w]
puts $mfh "* Sky130 mock model file"
puts $mfh ".model nfet_01v8 nmos level=1 VTO=0.4 KP=200u"
puts $mfh ".model pfet_01v8 pmos level=1 VTO=-0.4 KP=100u"
close $mfh

# Dynamically generate subckts for all cell types used in the design
set subckt_file [file join $spice_dir sky130_subckt.sp]
set sfh [open $subckt_file w]
puts $sfh "* Sky130 mock subckt file"

set cell_names [list]
set all_insts [get_cells *]
foreach inst $all_insts {
  set cell_ref [get_property $inst ref_name]
  if { [lsearch -exact $cell_names $cell_ref] == -1 } {
    lappend cell_names $cell_ref
  }
}

foreach cell_name $cell_names {
  set lib_pins [get_lib_pins */${cell_name}/*]
  if { [llength $lib_pins] == 0 } { continue }
  set ports [list]
  foreach lp $lib_pins {
    lappend ports [get_property $lp name]
  }
  if { [llength $ports] >= 2 } {
    puts $sfh ".subckt $cell_name [join $ports " "] VPWR VGND"
    puts $sfh "* mock transistor netlist"
    puts $sfh "R1 [lindex $ports 0] [lindex $ports 1] 1k"
    puts $sfh ".ends"
    puts $sfh ""
  }
}
close $sfh

#---------------------------------------------------------------
# write_path_spice with different simulators and path options
#---------------------------------------------------------------
puts "--- write_path_spice tests ---"

# Max path with ngspice
set pdir1 [make_result_file spice_gcd_path_ng]
file mkdir $pdir1
write_path_spice \
  -path_args {-sort_by_slack -path_delay max} \
  -spice_directory $pdir1 \
  -lib_subckt_file $subckt_file \
  -model_file $model_file \
  -power VPWR \
  -ground VGND \
  -simulator ngspice

# Min path with hspice
set pdir2 [make_result_file spice_gcd_path_hs]
file mkdir $pdir2
write_path_spice \
  -path_args {-path_delay min} \
  -spice_directory $pdir2 \
  -lib_subckt_file $subckt_file \
  -model_file $model_file \
  -power VPWR \
  -ground VGND \
  -simulator hspice

# Path with xyce
set pdir3 [make_result_file spice_gcd_path_xy]
file mkdir $pdir3
write_path_spice \
  -path_args {-sort_by_slack} \
  -spice_directory $pdir3 \
  -lib_subckt_file $subckt_file \
  -model_file $model_file \
  -power VPWR \
  -ground VGND \
  -simulator xyce
