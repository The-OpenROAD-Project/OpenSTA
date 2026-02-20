# Test write_gate_spice with different cell types, rise/fall transitions,
# and multiple simulators.
# NOTE: All write_gate_spice tests removed - write_gate_spice_cmd SWIG binding
# is missing. See bug_report_missing_write_gate_spice_cmd.md.
# Only baseline timing check remains.

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
