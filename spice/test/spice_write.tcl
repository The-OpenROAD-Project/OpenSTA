# Test spice-related functionality
source ../../test/helpers.tcl
set test_name spice_write

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog spice_test1.v
link_design spice_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_output_delay -clock clk 0 [get_ports out1]

# Run timing analysis (needed before write_path_spice)
report_checks

# Create mock SPICE model and subckt files for write_path_spice
set spice_dir [make_result_file spice_out]
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
puts $subckt_fh ".subckt DFF_X1 D CK Q VDD VSS"
puts $subckt_fh "M1 Q D VDD VDD pmos W=1u L=100n"
puts $subckt_fh "M2 Q D VSS VSS nmos W=1u L=100n"
puts $subckt_fh ".ends"
close $subckt_fh

# write_path_spice - exercises the Tcl command parsing and
# C++ WritePathSpice code paths.
write_path_spice \
  -path_args {-sort_by_slack} \
  -spice_directory $spice_dir \
  -lib_subckt_file $subckt_file \
  -model_file $model_file \
  -power VDD \
  -ground VSS
diff_files $test_name.spok [file join $spice_dir path_1.sp]
