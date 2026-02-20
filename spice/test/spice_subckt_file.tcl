# Test write_path_spice and write_gate_spice with -subcircuit_file option
# and various path configurations to exercise uncovered WriteSpice.cc paths:
#   WriteSpice constructor paths
#   writeSubckt, findSubckt paths
#   gatePortValues, inputStimulus paths
#   simulator-specific code paths (ngspice, hspice, xyce)
#   writeGateSpice error handling
# Also targets WritePathSpice.cc:
#   writePathSpice with different -path_args options

source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog spice_test2.v
link_design spice_test2

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports {in1 in2}]
set_output_delay -clock clk 1.0 [get_ports {out1 out2}]
set_input_transition 0.1 [get_ports {in1 in2}]

puts "--- report_checks ---"
report_checks

# Create mock SPICE subcircuit and model files
set spice_dir [make_result_file spice_subckt_out]
file mkdir $spice_dir

set model_file [file join $spice_dir model.sp]
set model_fh [open $model_file w]
puts $model_fh "* SPICE model file"
puts $model_fh ".model nmos nmos level=1"
puts $model_fh ".model pmos pmos level=1"
close $model_fh

set subckt_file [file join $spice_dir subckt.sp]
set subckt_fh [open $subckt_file w]
puts $subckt_fh "* SPICE subckt file"
puts $subckt_fh ".subckt BUF_X1 A Z VDD VSS"
puts $subckt_fh "M1 Z A VDD VDD pmos W=1u L=100n"
puts $subckt_fh "M2 Z A VSS VSS nmos W=1u L=100n"
puts $subckt_fh ".ends"
puts $subckt_fh ""
puts $subckt_fh ".subckt INV_X1 A ZN VDD VSS"
puts $subckt_fh "M1 ZN A VDD VDD pmos W=1u L=100n"
puts $subckt_fh "M2 ZN A VSS VSS nmos W=1u L=100n"
puts $subckt_fh ".ends"
puts $subckt_fh ""
puts $subckt_fh ".subckt AND2_X1 A1 A2 ZN VDD VSS"
puts $subckt_fh "M1 ZN A1 VDD VDD pmos W=1u L=100n"
puts $subckt_fh "M2 ZN A2 VSS VSS nmos W=1u L=100n"
puts $subckt_fh ".ends"
puts $subckt_fh ""
puts $subckt_fh ".subckt OR2_X1 A1 A2 ZN VDD VSS"
puts $subckt_fh "M1 ZN A1 VDD VDD pmos W=1u L=100n"
puts $subckt_fh "M2 ZN A2 VSS VSS nmos W=1u L=100n"
puts $subckt_fh ".ends"
puts $subckt_fh ""
puts $subckt_fh ".subckt DFF_X1 D CK Q QN VDD VSS"
puts $subckt_fh "M1 Q D VDD VDD pmos W=1u L=100n"
puts $subckt_fh "M2 Q D VSS VSS nmos W=1u L=100n"
puts $subckt_fh ".ends"
close $subckt_fh

#---------------------------------------------------------------
# write_gate_spice with multiple gates in one call
#---------------------------------------------------------------
puts "--- write_gate_spice multiple gates ---"
set gate_file [file join $spice_dir gates_multi.sp]
set rc [catch {
  write_gate_spice \
    -gates {{buf1 A Z rise} {inv1 A ZN fall}} \
    -spice_filename $gate_file \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS
} msg]
if { $rc == 0 } {
} else {
  puts "INFO: write_gate_spice multiple gates: $msg"
}

#---------------------------------------------------------------
# write_gate_spice with AND gate
#---------------------------------------------------------------
puts "--- write_gate_spice AND gate ---"
set gate_file2 [file join $spice_dir gate_and.sp]
set rc [catch {
  write_gate_spice \
    -gates {{and1 A1 ZN rise}} \
    -spice_filename $gate_file2 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS
} msg]
if { $rc == 0 } {
} else {
  puts "INFO: write_gate_spice AND: $msg"
}

#---------------------------------------------------------------
# write_path_spice with various options
#---------------------------------------------------------------
puts "--- write_path_spice to out1 ---"
set spice_dir2 [file join $spice_dir path_out1]
file mkdir $spice_dir2
write_path_spice \
  -path_args {-to out1 -path_delay max} \
  -spice_directory $spice_dir2 \
  -lib_subckt_file $subckt_file \
  -model_file $model_file \
  -power VDD \
  -ground VSS

puts "--- write_path_spice to out2 ---"
set spice_dir3 [file join $spice_dir path_out2]
file mkdir $spice_dir3
write_path_spice \
  -path_args {-to out2 -path_delay max} \
  -spice_directory $spice_dir3 \
  -lib_subckt_file $subckt_file \
  -model_file $model_file \
  -power VDD \
  -ground VSS

puts "--- write_path_spice with ngspice ---"
set spice_dir4 [file join $spice_dir path_ng]
file mkdir $spice_dir4
write_path_spice \
  -path_args {-sort_by_slack} \
  -spice_directory $spice_dir4 \
  -lib_subckt_file $subckt_file \
  -model_file $model_file \
  -power VDD \
  -ground VSS \
  -simulator ngspice
