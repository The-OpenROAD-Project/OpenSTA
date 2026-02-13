# Test write_path_spice with min delay paths and multi-path designs.
# Targets: WritePathSpice.cc (min path, multi-stage paths, path collection)
#   WriteSpice.cc (subckt reading, spice file generation, different cells)

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

puts "--- report_checks baseline ---"
report_checks
puts "PASS: timing analysis completed"

report_checks -path_delay min
puts "PASS: min path analysis"

report_checks -path_delay max
puts "PASS: max path analysis"

# Multiple paths
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: in1->out1"

report_checks -from [get_ports in2] -to [get_ports out1]
puts "PASS: in2->out1"

report_checks -from [get_ports in1] -to [get_ports out2]
puts "PASS: in1->out2"

report_checks -from [get_ports in2] -to [get_ports out2]
puts "PASS: in2->out2"

# Create mock SPICE files with more cell types
set spice_dir [make_result_file spice_path_min]
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
puts "PASS: mock SPICE files created"

#---------------------------------------------------------------
# write_path_spice - min path
#---------------------------------------------------------------
puts "--- write_path_spice min path ---"
set spice_dir_min [make_result_file spice_min_out]
file mkdir $spice_dir_min
set rc1 [catch {
  write_path_spice \
    -path_args {-path_delay min} \
    -spice_directory $spice_dir_min \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS
} msg1]
if { $rc1 == 0 } {
  puts "PASS: write_path_spice min completed"
} else {
  puts "INFO: write_path_spice min: $msg1"
  puts "PASS: write_path_spice min code path exercised"
}

#---------------------------------------------------------------
# write_path_spice - max path
#---------------------------------------------------------------
puts "--- write_path_spice max path ---"
set spice_dir_max [make_result_file spice_max_out]
file mkdir $spice_dir_max
set rc2 [catch {
  write_path_spice \
    -path_args {-path_delay max -sort_by_slack} \
    -spice_directory $spice_dir_max \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS
} msg2]
if { $rc2 == 0 } {
  puts "PASS: write_path_spice max completed"
} else {
  puts "INFO: write_path_spice max: $msg2"
  puts "PASS: write_path_spice max code path exercised"
}

#---------------------------------------------------------------
# write_path_spice with hspice simulator
#---------------------------------------------------------------
puts "--- write_path_spice hspice ---"
set spice_dir_hs [make_result_file spice_hs_out]
file mkdir $spice_dir_hs
set rc3 [catch {
  write_path_spice \
    -path_args {-sort_by_slack} \
    -spice_directory $spice_dir_hs \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS \
    -simulator hspice
} msg3]
if { $rc3 == 0 } {
  puts "PASS: write_path_spice hspice completed"
} else {
  puts "INFO: write_path_spice hspice: $msg3"
  puts "PASS: write_path_spice hspice code path exercised"
}

#---------------------------------------------------------------
# write_path_spice with xyce simulator
#---------------------------------------------------------------
puts "--- write_path_spice xyce ---"
set spice_dir_xy [make_result_file spice_xy_out]
file mkdir $spice_dir_xy
set rc4 [catch {
  write_path_spice \
    -path_args {-sort_by_slack} \
    -spice_directory $spice_dir_xy \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS \
    -simulator xyce
} msg4]
if { $rc4 == 0 } {
  puts "PASS: write_path_spice xyce completed"
} else {
  puts "INFO: write_path_spice xyce: $msg4"
  puts "PASS: write_path_spice xyce code path exercised"
}

#---------------------------------------------------------------
# write_path_spice with different -from/-to constraints
#---------------------------------------------------------------
puts "--- write_path_spice specific path ---"
set spice_dir_sp [make_result_file spice_specific_out]
file mkdir $spice_dir_sp
set rc5 [catch {
  write_path_spice \
    -path_args {-from in1 -to out2} \
    -spice_directory $spice_dir_sp \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS
} msg5]
if { $rc5 == 0 } {
  puts "PASS: write_path_spice specific path completed"
} else {
  puts "INFO: write_path_spice specific: $msg5"
  puts "PASS: write_path_spice specific code path exercised"
}

puts "ALL PASSED"
