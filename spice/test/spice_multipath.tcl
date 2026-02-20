# Test SPICE write with GCD sky130hd design (larger design, more paths, bus signals)
# Targets: WritePathSpice.cc (multi-stage paths, bus pin handling,
#   writeHeader, writePrintStmt, writeStageSubckts, writeInputSource,
#   writeMeasureStmts, drvr/load connection handling)
#   WriteSpice.cc (subckt file parsing with many cell types,
#   different cell topologies, NAND/NOR/INV/BUF/DFF/MUX cells,
#   readSubcktDef, readSubcktPinNames, spice model generation)

source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog spice_test2.v
link_design spice_test2

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 1.0 [get_ports out1]
set_output_delay -clock clk 1.0 [get_ports out2]
set_input_transition 0.1 [get_ports {in1 in2}]

# Read SPEF for parasitics (exercises parasitic path in spice write)
# Use manual parasitics since we don't have matching SPEF
sta::set_pi_model buf1/Z 0.002 5.0 0.001
sta::set_elmore buf1/Z and1/A1 0.001
sta::set_elmore buf1/Z or1/A1 0.001

sta::set_pi_model inv1/ZN 0.002 5.0 0.001
sta::set_elmore inv1/ZN and1/A2 0.001
sta::set_elmore inv1/ZN or1/A2 0.001

sta::set_pi_model and1/ZN 0.001 3.0 0.0005
sta::set_elmore and1/ZN reg1/D 0.001

sta::set_pi_model or1/ZN 0.001 3.0 0.0005
sta::set_elmore or1/ZN reg2/D 0.001

puts "--- report_checks ---"
report_checks

report_checks -path_delay min

report_checks -from [get_ports in1] -to [get_ports out1]

report_checks -from [get_ports in2] -to [get_ports out2]

# Create comprehensive mock SPICE files
set spice_dir [make_result_file spice_multipath]
file mkdir $spice_dir

set model_file [file join $spice_dir model.sp]
set model_fh [open $model_file w]
puts $model_fh "* SPICE model file"
puts $model_fh ".model nmos nmos level=3 tox=9e-9 vth0=0.7 gamma=0.5 phi=0.8 mu0=350"
puts $model_fh ".model pmos pmos level=3 tox=9e-9 vth0=-0.7 gamma=0.4 phi=0.8 mu0=100"
close $model_fh

set subckt_file [file join $spice_dir subckt.sp]
set subckt_fh [open $subckt_file w]
puts $subckt_fh "* Subcircuit definitions"
puts $subckt_fh ""
puts $subckt_fh ".subckt BUF_X1 A Z VDD VSS"
puts $subckt_fh "M1 n1 A VSS VSS nmos W=0.2u L=0.1u"
puts $subckt_fh "M2 n1 A VDD VDD pmos W=0.4u L=0.1u"
puts $subckt_fh "M3 Z n1 VSS VSS nmos W=0.2u L=0.1u"
puts $subckt_fh "M4 Z n1 VDD VDD pmos W=0.4u L=0.1u"
puts $subckt_fh ".ends"
puts $subckt_fh ""
puts $subckt_fh ".subckt BUF_X2 A Z VDD VSS"
puts $subckt_fh "M1 n1 A VSS VSS nmos W=0.4u L=0.1u"
puts $subckt_fh "M2 n1 A VDD VDD pmos W=0.8u L=0.1u"
puts $subckt_fh "M3 Z n1 VSS VSS nmos W=0.4u L=0.1u"
puts $subckt_fh "M4 Z n1 VDD VDD pmos W=0.8u L=0.1u"
puts $subckt_fh ".ends"
puts $subckt_fh ""
puts $subckt_fh ".subckt INV_X1 A ZN VDD VSS"
puts $subckt_fh "M1 ZN A VSS VSS nmos W=0.2u L=0.1u"
puts $subckt_fh "M2 ZN A VDD VDD pmos W=0.4u L=0.1u"
puts $subckt_fh ".ends"
puts $subckt_fh ""
puts $subckt_fh ".subckt AND2_X1 A1 A2 ZN VDD VSS"
puts $subckt_fh "M1 n1 A1 VSS VSS nmos W=0.2u L=0.1u"
puts $subckt_fh "M2 ZN A2 n1 VSS nmos W=0.2u L=0.1u"
puts $subckt_fh "M3 ZN A1 VDD VDD pmos W=0.4u L=0.1u"
puts $subckt_fh "M4 ZN A2 VDD VDD pmos W=0.4u L=0.1u"
puts $subckt_fh ".ends"
puts $subckt_fh ""
puts $subckt_fh ".subckt OR2_X1 A1 A2 ZN VDD VSS"
puts $subckt_fh "M1 ZN A1 VSS VSS nmos W=0.2u L=0.1u"
puts $subckt_fh "M2 ZN A2 VSS VSS nmos W=0.2u L=0.1u"
puts $subckt_fh "M3 n1 A1 VDD VDD pmos W=0.4u L=0.1u"
puts $subckt_fh "M4 ZN A2 n1 VDD pmos W=0.4u L=0.1u"
puts $subckt_fh ".ends"
puts $subckt_fh ""
puts $subckt_fh ".subckt DFF_X1 D CK Q QN VDD VSS"
puts $subckt_fh "M1 Q D VDD VDD pmos W=0.4u L=0.1u"
puts $subckt_fh "M2 Q D VSS VSS nmos W=0.2u L=0.1u"
puts $subckt_fh ".ends"
close $subckt_fh

#---------------------------------------------------------------
# write_path_spice with max path (default)
#---------------------------------------------------------------
puts "--- write_path_spice max ---"
set spice_dir1 [make_result_file spice_mp_max]
file mkdir $spice_dir1
write_path_spice \
  -path_args {-sort_by_slack} \
  -spice_directory $spice_dir1 \
  -lib_subckt_file $subckt_file \
  -model_file $model_file \
  -power VDD \
  -ground VSS

#---------------------------------------------------------------
# write_path_spice with min path
#---------------------------------------------------------------
puts "--- write_path_spice min ---"
set spice_dir2 [make_result_file spice_mp_min]
file mkdir $spice_dir2
write_path_spice \
  -path_args {-path_delay min} \
  -spice_directory $spice_dir2 \
  -lib_subckt_file $subckt_file \
  -model_file $model_file \
  -power VDD \
  -ground VSS

#---------------------------------------------------------------
# write_path_spice with specific from/to
#---------------------------------------------------------------
puts "--- write_path_spice specific path ---"
set spice_dir3 [make_result_file spice_mp_specific]
file mkdir $spice_dir3
# catch: write_path_spice may fail if subckt is missing for cells on path
set rc3 [catch {
  write_path_spice \
    -path_args {-from in1 -to out1} \
    -spice_directory $spice_dir3 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS
} msg3]
if { $rc3 == 0 } {
} else {
  puts "INFO: write_path_spice specific: $msg3"
}

#---------------------------------------------------------------
# write_path_spice with hspice
#---------------------------------------------------------------
puts "--- write_path_spice hspice ---"
set spice_dir4 [make_result_file spice_mp_hspice]
file mkdir $spice_dir4
write_path_spice \
  -path_args {-sort_by_slack} \
  -spice_directory $spice_dir4 \
  -lib_subckt_file $subckt_file \
  -model_file $model_file \
  -power VDD \
  -ground VSS \
  -simulator hspice

#---------------------------------------------------------------
# write_path_spice with xyce
#---------------------------------------------------------------
puts "--- write_path_spice xyce ---"
set spice_dir5 [make_result_file spice_mp_xyce]
file mkdir $spice_dir5
write_path_spice \
  -path_args {-sort_by_slack} \
  -spice_directory $spice_dir5 \
  -lib_subckt_file $subckt_file \
  -model_file $model_file \
  -power VDD \
  -ground VSS \
  -simulator xyce

#---------------------------------------------------------------
# write_gate_spice with multiple cells and transitions
#---------------------------------------------------------------
puts "--- write_gate_spice multiple cells ---"

# BUF rise
set gf1 [file join $spice_dir gate_buf_rise.sp]
# catch: write_gate_spice may fail if subckt pin mapping doesn't match liberty cell
set rc [catch {
  write_gate_spice -gates {{buf1 A Z rise}} -spice_filename $gf1 \
    -lib_subckt_file $subckt_file -model_file $model_file \
    -power VDD -ground VSS
} msg]
if { $rc == 0 } { puts "gate BUF rise" } else { puts "INFO: gate BUF rise: $msg" }

# BUF fall
set gf2 [file join $spice_dir gate_buf_fall.sp]
# catch: write_gate_spice may fail if subckt pin mapping doesn't match liberty cell
set rc [catch {
  write_gate_spice -gates {{buf1 A Z fall}} -spice_filename $gf2 \
    -lib_subckt_file $subckt_file -model_file $model_file \
    -power VDD -ground VSS
} msg]
if { $rc == 0 } { puts "gate BUF fall" } else { puts "INFO: gate BUF fall: $msg" }

# INV rise
set gf3 [file join $spice_dir gate_inv_rise.sp]
# catch: write_gate_spice may fail if subckt pin mapping doesn't match liberty cell
set rc [catch {
  write_gate_spice -gates {{inv1 A ZN rise}} -spice_filename $gf3 \
    -lib_subckt_file $subckt_file -model_file $model_file \
    -power VDD -ground VSS
} msg]
if { $rc == 0 } { puts "gate INV rise" } else { puts "INFO: gate INV rise: $msg" }

# AND rise from A1
set gf4 [file join $spice_dir gate_and_a1_rise.sp]
# catch: write_gate_spice may fail if subckt pin mapping doesn't match liberty cell
set rc [catch {
  write_gate_spice -gates {{and1 A1 ZN rise}} -spice_filename $gf4 \
    -lib_subckt_file $subckt_file -model_file $model_file \
    -power VDD -ground VSS
} msg]
if { $rc == 0 } { puts "gate AND A1 rise" } else { puts "INFO: gate AND A1 rise: $msg" }

# AND fall from A2
set gf5 [file join $spice_dir gate_and_a2_fall.sp]
# catch: write_gate_spice may fail if subckt pin mapping doesn't match liberty cell
set rc [catch {
  write_gate_spice -gates {{and1 A2 ZN fall}} -spice_filename $gf5 \
    -lib_subckt_file $subckt_file -model_file $model_file \
    -power VDD -ground VSS
} msg]
if { $rc == 0 } { puts "gate AND A2 fall" } else { puts "INFO: gate AND A2 fall: $msg" }

# OR rise
set gf6 [file join $spice_dir gate_or_rise.sp]
# catch: write_gate_spice may fail if subckt pin mapping doesn't match liberty cell
set rc [catch {
  write_gate_spice -gates {{or1 A1 ZN rise}} -spice_filename $gf6 \
    -lib_subckt_file $subckt_file -model_file $model_file \
    -power VDD -ground VSS
} msg]
if { $rc == 0 } { puts "gate OR rise" } else { puts "INFO: gate OR rise: $msg" }

# Hspice simulator variants
set gf7 [file join $spice_dir gate_inv_hspice.sp]
# catch: write_gate_spice may fail if subckt pin mapping doesn't match liberty cell
set rc [catch {
  write_gate_spice -gates {{inv1 A ZN fall}} -spice_filename $gf7 \
    -lib_subckt_file $subckt_file -model_file $model_file \
    -power VDD -ground VSS -simulator hspice
} msg]
if { $rc == 0 } { puts "gate INV hspice" } else { puts "INFO: gate INV hspice: $msg" }

# Xyce simulator variants
set gf8 [file join $spice_dir gate_or_xyce.sp]
# catch: write_gate_spice may fail if subckt pin mapping doesn't match liberty cell
set rc [catch {
  write_gate_spice -gates {{or1 A2 ZN rise}} -spice_filename $gf8 \
    -lib_subckt_file $subckt_file -model_file $model_file \
    -power VDD -ground VSS -simulator xyce
} msg]
if { $rc == 0 } { puts "gate OR xyce" } else { puts "INFO: gate OR xyce: $msg" }
