# Test advanced SPICE writing: write_gate_spice with various options,
# multiple simulators, min/max paths, write_path_spice with -path_args options.
# Targets uncovered WriteSpice.cc and WritePathSpice.cc paths.

source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog spice_test1.v
link_design spice_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_output_delay -clock clk 1.0 [get_ports out1]
set_input_transition 0.1 [get_ports in1]

puts "--- report_checks baseline ---"
report_checks

# Create mock SPICE files
set spice_dir [make_result_file spice_adv_out]
file mkdir $spice_dir

set model_file [file join $spice_dir mock_model.sp]
set model_fh [open $model_file w]
puts $model_fh "* Mock SPICE model file"
puts $model_fh ".model nmos nmos level=1"
puts $model_fh ".model pmos pmos level=1"
close $model_fh

set subckt_file [file join $spice_dir mock_subckt.sp]
set subckt_fh [open $subckt_file w]
puts $subckt_fh "* Mock SPICE subckt file"
puts $subckt_fh ".subckt BUF_X1 A Z VDD VSS"
puts $subckt_fh "M1 Z A VDD VDD pmos W=1u L=100n"
puts $subckt_fh "M2 Z A VSS VSS nmos W=1u L=100n"
puts $subckt_fh ".ends"
puts $subckt_fh ""
puts $subckt_fh ".subckt BUF_X2 A Z VDD VSS"
puts $subckt_fh "M1 Z A VDD VDD pmos W=1u L=100n"
puts $subckt_fh "M2 Z A VSS VSS nmos W=1u L=100n"
puts $subckt_fh ".ends"
puts $subckt_fh ""
puts $subckt_fh ".subckt DFF_X1 D CK Q QN VDD VSS"
puts $subckt_fh "M1 Q D VDD VDD pmos W=1u L=100n"
puts $subckt_fh "M2 Q D VSS VSS nmos W=1u L=100n"
puts $subckt_fh ".ends"
close $subckt_fh

#---------------------------------------------------------------
# write_gate_spice - ngspice (default)
#---------------------------------------------------------------
puts "--- write_gate_spice ngspice ---"
set gate_file1 [file join $spice_dir gate_ng.sp]
set rc1 [catch {
  write_gate_spice \
    -gates {{buf1 A Z rise}} \
    -spice_filename $gate_file1 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS
} msg1]
if { $rc1 == 0 } {
} else {
  puts "INFO: write_gate_spice ngspice: $msg1"
}

#---------------------------------------------------------------
# write_gate_spice - fall transition
#---------------------------------------------------------------
puts "--- write_gate_spice fall ---"
set gate_file2 [file join $spice_dir gate_fall.sp]
set rc2 [catch {
  write_gate_spice \
    -gates {{buf1 A Z fall}} \
    -spice_filename $gate_file2 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS \
    -simulator ngspice
} msg2]
if { $rc2 == 0 } {
} else {
  puts "INFO: write_gate_spice fall: $msg2"
}

#---------------------------------------------------------------
# write_gate_spice - xyce simulator
#---------------------------------------------------------------
puts "--- write_gate_spice xyce ---"
set gate_file3 [file join $spice_dir gate_xyce.sp]
set rc3 [catch {
  write_gate_spice \
    -gates {{buf1 A Z rise}} \
    -spice_filename $gate_file3 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS \
    -simulator xyce
} msg3]
if { $rc3 == 0 } {
} else {
  puts "INFO: write_gate_spice xyce: $msg3"
}

#---------------------------------------------------------------
# write_gate_spice - hspice simulator
#---------------------------------------------------------------
puts "--- write_gate_spice hspice ---"
set gate_file4 [file join $spice_dir gate_hspice.sp]
set rc4 [catch {
  write_gate_spice \
    -gates {{buf1 A Z rise}} \
    -spice_filename $gate_file4 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS \
    -simulator hspice
} msg4]
if { $rc4 == 0 } {
} else {
  puts "INFO: write_gate_spice hspice: $msg4"
}

#---------------------------------------------------------------
# write_path_spice with -path_args max slack
#---------------------------------------------------------------
puts "--- write_path_spice max slack ---"
set spice_dir2 [make_result_file spice_adv_path]
file mkdir $spice_dir2
set rc5 [catch {
  write_path_spice \
    -path_args {-sort_by_slack -path_delay max} \
    -spice_directory $spice_dir2 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS
} msg5]
if { $rc5 == 0 } {
} else {
  puts "INFO: write_path_spice max slack: $msg5"
}

#---------------------------------------------------------------
# write_path_spice min path
#---------------------------------------------------------------
puts "--- write_path_spice min path ---"
set spice_dir3 [make_result_file spice_adv_min]
file mkdir $spice_dir3
set rc6 [catch {
  write_path_spice \
    -path_args {-path_delay min} \
    -spice_directory $spice_dir3 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS
} msg6]
if { $rc6 == 0 } {
} else {
  puts "INFO: write_path_spice min: $msg6"
}

#---------------------------------------------------------------
# write_path_spice with hspice
#---------------------------------------------------------------
puts "--- write_path_spice hspice ---"
set spice_dir4 [make_result_file spice_adv_hspice]
file mkdir $spice_dir4
set rc7 [catch {
  write_path_spice \
    -path_args {-sort_by_slack} \
    -spice_directory $spice_dir4 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS \
    -simulator hspice
} msg7]
if { $rc7 == 0 } {
} else {
  puts "INFO: write_path_spice hspice: $msg7"
}

#---------------------------------------------------------------
# write_path_spice with xyce
#---------------------------------------------------------------
puts "--- write_path_spice xyce ---"
set spice_dir5 [make_result_file spice_adv_xyce]
file mkdir $spice_dir5
set rc8 [catch {
  write_path_spice \
    -path_args {-sort_by_slack} \
    -spice_directory $spice_dir5 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS \
    -simulator xyce
} msg8]
if { $rc8 == 0 } {
} else {
  puts "INFO: write_path_spice xyce: $msg8"
}
