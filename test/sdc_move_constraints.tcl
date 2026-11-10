# Constraints on a replaced instance and its pins move to the replacement.
source helpers.tcl
read_liberty asap7_small.lib.gz
read_verilog sdc_move_constraints.v
link_design sdc_move_constraints
create_clock -name clk -period 100 [get_ports clk]
create_generated_clock -name div -source [get_pins reg1/CLK] -divide_by 2 \
  [get_pins reg1/Q]
set_clock_latency -source 5 -clock div [get_pins reg1/Q]
set_clock_latency 10 [get_pins reg1/CLK]
set_propagated_clock [get_pins reg1/Q]
set_clock_uncertainty 0.5 [get_pins reg1/CLK]
set_sense -type clock -stop_propagation -clocks clk [get_pins reg1/Q]
set_clock_gating_check -setup 0.2 [get_pins reg1/CLK]
set_clock_gating_check -hold 0.1 [get_cells reg1]
set_data_check -from [get_pins reg1/CLK] -to [get_pins reg1/D] -setup 0.3
set_min_pulse_width 2 [get_pins reg1/CLK]
set_min_pulse_width 3 [get_cells reg1]
set_max_time_borrow 1 [get_pins reg1/D]
set_max_time_borrow 2 [get_cells reg1]
set_max_capacitance 0.5 [get_pins reg1/Q]
set_disable_timing [get_pins reg1/D]
set_disable_timing -from CLK -to Q [get_cells reg1]
set_disable_timing [get_timing_edges -from reg1/CLK -to reg1/Q]
set_timing_derate -cell_delay -late 1.1 [get_cells reg1]
set_case_analysis 0 [get_pins reg1/D]
set_input_delay 1 -clock clk -reference_pin [get_pins reg1/CLK] [get_ports in2]
set_false_path -from [get_pins reg1/CLK] -to [get_ports out1]
set_multicycle_path 2 -setup -to [get_pins reg1/D]
set_max_delay 5 -through [get_pins reg1/Q] -to [get_ports out1]
set_multicycle_path 3 -hold -from [get_cells reg1]
set sdc_file [make_result_file sdc_move_constraints.sdc]
write_sdc -no_timestamp $sdc_file
report_file $sdc_file

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
write_sdc -no_timestamp $sdc_file
report_file $sdc_file
