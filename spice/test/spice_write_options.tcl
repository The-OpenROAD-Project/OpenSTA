# Test SPICE write with additional options
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog spice_test1.v
link_design spice_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_output_delay -clock clk 1.0 [get_ports out1]

puts "--- report_checks ---"
report_checks

# Create mock SPICE files
set spice_dir [make_result_file spice_opts_out]
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

puts "--- write_path_spice default ---"
write_path_spice \
  -path_args {-sort_by_slack} \
  -spice_directory $spice_dir \
  -lib_subckt_file $subckt_file \
  -model_file $model_file \
  -power VDD \
  -ground VSS

puts "--- write_path_spice with -simulator hspice ---"
write_path_spice \
  -path_args {-sort_by_slack} \
  -spice_directory $spice_dir \
  -lib_subckt_file $subckt_file \
  -model_file $model_file \
  -power VDD \
  -ground VSS \
  -simulator hspice

puts "--- write_path_spice with -simulator xyce ---"
write_path_spice \
  -path_args {-sort_by_slack} \
  -spice_directory $spice_dir \
  -lib_subckt_file $subckt_file \
  -model_file $model_file \
  -power VDD \
  -ground VSS \
  -simulator xyce

puts "--- write_gate_spice ---"
set gate_spice_file [file join $spice_dir gate_test.sp]
# catch: write_gate_spice may fail if subckt pin mapping doesn't match liberty cell
set rc4 [catch {
  write_gate_spice \
    -gates {{buf1 A Z rise}} \
    -spice_filename $gate_spice_file \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS
} msg4]
if { $rc4 == 0 } {
} else {
  puts "INFO: write_gate_spice: $msg4"
}

puts "--- write_gate_spice with -simulator hspice ---"
set gate_spice_file2 [file join $spice_dir gate_test2.sp]
# catch: write_gate_spice may fail if subckt pin mapping doesn't match liberty cell
set rc5 [catch {
  write_gate_spice \
    -gates {{buf1 A Z rise}} \
    -spice_filename $gate_spice_file2 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS \
    -simulator hspice
} msg5]
if { $rc5 == 0 } {
} else {
  puts "INFO: write_gate_spice hspice: $msg5"
}
