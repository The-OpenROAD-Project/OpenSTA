# Constraints in an analysis corner overlay move with the pins and
# instances they reference, like the mode's constraints, when an
# application replaces an instance with move_*_constraints.
source ../../test/helpers.tcl
read_liberty ../../test/asap7_small.lib.gz
read_verilog ../../test/sdc_move_constraints.v
link_design sdc_move_constraints

create_clock -name clk -period 100 [get_ports clk]
set_input_delay 1.0 -clock clk [get_ports {in1 in2}]
set_output_delay 2.0 -clock clk [get_ports out1]

# Corner-only internal input delay on reg1/Q.
set corner_sdc [make_result_file corner_move_constraints.sdc]
set stream [open $corner_sdc "w"]
puts $stream {set_input_delay 90 -clock clk [get_pins reg1/Q]}
close $stream
define_analysis_corner ac -liberty asap7_small -sdc [list $corner_sdc]
define_scene sc -analysis_corner ac

puts "=== before move: corner input delay on reg1/Q ==="
report_checks -from [get_pins reg1/Q]

# Replace reg1 with reg2 as an application would.
make_instance reg2 DFFHQx4_ASAP7_75t_R
foreach port {D CLK Q} {
  sta::move_pin_constraints_cmd [get_pins reg1/$port] [get_pins reg2/$port]
}
sta::move_instance_constraints_cmd [get_cells reg1] [get_cells reg2]
delete_instance reg1
connect_pin n1 reg2/D
connect_pin clk reg2/CLK
connect_pin n2 reg2/Q

puts "=== after move: corner input delay on reg2/Q ==="
report_checks -from [get_pins reg2/Q]
