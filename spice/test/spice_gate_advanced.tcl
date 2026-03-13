# Test advanced SPICE writing: write_path_spice with -path_args options
# and various simulators.
# NOTE: write_gate_spice tests removed - write_gate_spice_cmd SWIG binding
# is missing. See bug_report_missing_write_gate_spice_cmd.md.

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
# write_path_spice with -path_args max slack
#---------------------------------------------------------------
puts "--- write_path_spice max slack ---"
set spice_dir2 [make_result_file spice_adv_path]
file mkdir $spice_dir2
write_path_spice \
  -path_args {-sort_by_slack -path_delay max} \
  -spice_directory $spice_dir2 \
  -lib_subckt_file $subckt_file \
  -model_file $model_file \
  -power VDD \
  -ground VSS

#---------------------------------------------------------------
# write_path_spice min path
#---------------------------------------------------------------
puts "--- write_path_spice min path ---"
set spice_dir3 [make_result_file spice_adv_min]
file mkdir $spice_dir3
write_path_spice \
  -path_args {-path_delay min} \
  -spice_directory $spice_dir3 \
  -lib_subckt_file $subckt_file \
  -model_file $model_file \
  -power VDD \
  -ground VSS

#---------------------------------------------------------------
# write_path_spice with hspice
#---------------------------------------------------------------
puts "--- write_path_spice hspice ---"
set spice_dir4 [make_result_file spice_adv_hspice]
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
set spice_dir5 [make_result_file spice_adv_xyce]
file mkdir $spice_dir5
write_path_spice \
  -path_args {-sort_by_slack} \
  -spice_directory $spice_dir5 \
  -lib_subckt_file $subckt_file \
  -model_file $model_file \
  -power VDD \
  -ground VSS \
  -simulator xyce
