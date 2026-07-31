# write_path_spice ties non-unate side inputs to the sensitized arc (issue #474)
source helpers.tcl
read_liberty write_path_spice_arc_sense.lib.gz
read_verilog write_path_spice_arc_sense.v
link_design repro
create_clock -name vclk -period 10
set_input_delay -clock vclk 0 [all_inputs]
set_output_delay -clock vclk 0 [all_outputs]
# Force the inverting arc B(fall) -> X(rise), which the liberty defines only
# under "when A" (A=1).  The xor2 side input A is the unconstrained port s, so
# a correct deck must tie x0/A high (v1 x0/A 0 1.800).  Before the fix the
# Boolean-difference cube ignored the arc direction and tied it low (0.000).
set spice_file [make_result_file "write_path_spice_arc_sense.sp"]
write_path_spice -path_args {-path_delay max -fall_from [get_ports a] -rise_to [get_ports x]} \
  -spice_file $spice_file \
  -lib_subckt_file write_path_spice_arc_sense.cells.spice \
  -model_file write_path_spice_arc_sense.models.spice \
  -power VPWR -ground VGND \
  -simulator ngspice
report_file ${spice_file}_1.sp
