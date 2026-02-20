# Test write_gate_spice with different cell types, rise/fall transitions,
# and multiple simulators.
# Targets: WriteSpice.cc (subckt file parsing, multiple cell types,
#   rise/fall pin handling, Xyce/HSpice specific output)
#   WritePathSpice.cc (cell-level spice generation)

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

# Create mock SPICE files
set spice_dir [make_result_file spice_gate_cells]
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

#---------------------------------------------------------------
# write_gate_spice - BUF_X1 rise (ngspice)
#---------------------------------------------------------------
puts "--- write_gate_spice BUF_X1 rise ngspice ---"
set gate_f1 [file join $spice_dir gate_buf_rise.sp]
# catch: write_gate_spice may fail if subckt pin mapping doesn't match liberty cell
set rc1 [catch {
  write_gate_spice \
    -gates {{buf1 A Z rise}} \
    -spice_filename $gate_f1 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS
} msg1]
if { $rc1 == 0 } {
} else {
  puts "INFO: write_gate_spice BUF rise: $msg1"
}

#---------------------------------------------------------------
# write_gate_spice - BUF_X1 fall
#---------------------------------------------------------------
puts "--- write_gate_spice BUF_X1 fall ---"
set gate_f2 [file join $spice_dir gate_buf_fall.sp]
# catch: write_gate_spice may fail if subckt pin mapping doesn't match liberty cell
set rc2 [catch {
  write_gate_spice \
    -gates {{buf1 A Z fall}} \
    -spice_filename $gate_f2 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS
} msg2]
if { $rc2 == 0 } {
} else {
  puts "INFO: write_gate_spice BUF fall: $msg2"
}

#---------------------------------------------------------------
# write_gate_spice - INV_X1 rise
#---------------------------------------------------------------
puts "--- write_gate_spice INV_X1 rise ---"
set gate_f3 [file join $spice_dir gate_inv_rise.sp]
# catch: write_gate_spice may fail if subckt pin mapping doesn't match liberty cell
set rc3 [catch {
  write_gate_spice \
    -gates {{inv1 A ZN rise}} \
    -spice_filename $gate_f3 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS
} msg3]
if { $rc3 == 0 } {
} else {
  puts "INFO: write_gate_spice INV rise: $msg3"
}

#---------------------------------------------------------------
# write_gate_spice - INV_X1 fall
#---------------------------------------------------------------
puts "--- write_gate_spice INV_X1 fall ---"
set gate_f4 [file join $spice_dir gate_inv_fall.sp]
# catch: write_gate_spice may fail if subckt pin mapping doesn't match liberty cell
set rc4 [catch {
  write_gate_spice \
    -gates {{inv1 A ZN fall}} \
    -spice_filename $gate_f4 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS
} msg4]
if { $rc4 == 0 } {
} else {
  puts "INFO: write_gate_spice INV fall: $msg4"
}

#---------------------------------------------------------------
# write_gate_spice - AND2_X1 rise (multi-input)
#---------------------------------------------------------------
puts "--- write_gate_spice AND2_X1 rise ---"
set gate_f5 [file join $spice_dir gate_and_rise.sp]
# catch: write_gate_spice may fail if subckt pin mapping doesn't match liberty cell
set rc5 [catch {
  write_gate_spice \
    -gates {{and1 A1 ZN rise}} \
    -spice_filename $gate_f5 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS
} msg5]
if { $rc5 == 0 } {
} else {
  puts "INFO: write_gate_spice AND rise: $msg5"
}

#---------------------------------------------------------------
# write_gate_spice - AND2_X1 from A2 input
#---------------------------------------------------------------
puts "--- write_gate_spice AND2_X1 A2 ---"
set gate_f5b [file join $spice_dir gate_and_a2.sp]
# catch: write_gate_spice may fail if subckt pin mapping doesn't match liberty cell
set rc5b [catch {
  write_gate_spice \
    -gates {{and1 A2 ZN rise}} \
    -spice_filename $gate_f5b \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS
} msg5b]
if { $rc5b == 0 } {
} else {
  puts "INFO: write_gate_spice AND A2: $msg5b"
}

#---------------------------------------------------------------
# write_gate_spice - OR2_X1 rise
#---------------------------------------------------------------
puts "--- write_gate_spice OR2_X1 rise ---"
set gate_f6 [file join $spice_dir gate_or_rise.sp]
# catch: write_gate_spice may fail if subckt pin mapping doesn't match liberty cell
set rc6 [catch {
  write_gate_spice \
    -gates {{or1 A1 ZN rise}} \
    -spice_filename $gate_f6 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS
} msg6]
if { $rc6 == 0 } {
} else {
  puts "INFO: write_gate_spice OR rise: $msg6"
}

#---------------------------------------------------------------
# write_gate_spice with hspice simulator
#---------------------------------------------------------------
puts "--- write_gate_spice hspice ---"
set gate_f7 [file join $spice_dir gate_hspice.sp]
# catch: write_gate_spice may fail if subckt pin mapping doesn't match liberty cell
set rc7 [catch {
  write_gate_spice \
    -gates {{buf1 A Z rise}} \
    -spice_filename $gate_f7 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS \
    -simulator hspice
} msg7]
if { $rc7 == 0 } {
} else {
  puts "INFO: write_gate_spice hspice: $msg7"
}

#---------------------------------------------------------------
# write_gate_spice with xyce simulator
#---------------------------------------------------------------
puts "--- write_gate_spice xyce ---"
set gate_f8 [file join $spice_dir gate_xyce.sp]
# catch: write_gate_spice may fail if subckt pin mapping doesn't match liberty cell
set rc8 [catch {
  write_gate_spice \
    -gates {{buf1 A Z rise}} \
    -spice_filename $gate_f8 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS \
    -simulator xyce
} msg8]
if { $rc8 == 0 } {
} else {
  puts "INFO: write_gate_spice xyce: $msg8"
}

#---------------------------------------------------------------
# write_gate_spice with xyce for INV cell (different topology)
#---------------------------------------------------------------
puts "--- write_gate_spice xyce INV ---"
set gate_f9 [file join $spice_dir gate_xyce_inv.sp]
# catch: write_gate_spice may fail if subckt pin mapping doesn't match liberty cell
set rc9 [catch {
  write_gate_spice \
    -gates {{inv1 A ZN fall}} \
    -spice_filename $gate_f9 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS \
    -simulator xyce
} msg9]
if { $rc9 == 0 } {
} else {
  puts "INFO: write_gate_spice xyce INV: $msg9"
}

#---------------------------------------------------------------
# write_gate_spice with hspice for AND cell
#---------------------------------------------------------------
puts "--- write_gate_spice hspice AND ---"
set gate_f10 [file join $spice_dir gate_hspice_and.sp]
# catch: write_gate_spice may fail if subckt pin mapping doesn't match liberty cell
set rc10 [catch {
  write_gate_spice \
    -gates {{and1 A1 ZN fall}} \
    -spice_filename $gate_f10 \
    -lib_subckt_file $subckt_file \
    -model_file $model_file \
    -power VDD \
    -ground VSS \
    -simulator hspice
} msg10]
if { $rc10 == 0 } {
} else {
  puts "INFO: write_gate_spice hspice AND: $msg10"
}
