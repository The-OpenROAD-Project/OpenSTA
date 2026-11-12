# Constraints and emptied exceptions on deleted pins and instances are removed.
source helpers.tcl
read_liberty asap7_small.lib.gz
read_verilog power_pin_delete.v
link_design power_pin_delete
create_clock -name clk -period 10 [get_ports clk]
# Pin constraints on reg1
set_false_path -to [get_pins reg1/D]
set_false_path -through [get_pins {buf1/Y reg1/Q}]
set_case_analysis 0 [get_pins reg1/D]
set_disable_timing [get_pins reg1/Q]
set_disable_timing [get_timing_edges -from [get_pins reg1/CLK] -to [get_pins reg1/Q]]
set_clock_latency 0.1 [get_pins reg1/CLK]
create_generated_clock -name div -source [get_ports clk] -divide_by 2 [get_pins {reg1/Q buf1/Y}]
set_clock_latency -source 0.2 [get_pins reg1/Q]
set_propagated_clock [get_pins reg1/D]
set_clock_uncertainty 0.05 [get_pins reg1/CLK]
set_sense -type clock -stop_propagation -clocks clk [get_pins reg1/CLK]
set_clock_gating_check -setup 0.1 [get_pins reg1/CLK]
set_disable_clock_gating_check [get_pins reg1/CLK]
set_data_check -from [get_pins reg1/CLK] -to [get_pins reg1/D] -setup 0.2
set_min_pulse_width 0.3 [get_pins reg1/CLK]
set_max_time_borrow 0.4 [get_pins reg1/D]
set_max_capacitance 0.1 [get_pins reg1/Q]
# Generated clock whose -source pin is deleted.
create_generated_clock -name div_ck -source [get_pins reg1/CLK] \
  -divide_by 2 [get_ports out1]
# Instance constraints on reg1
set_multicycle_path -setup 2 -from [get_cells reg1]
set_timing_derate -cell_delay -late 1.1 [get_cells reg1]
set_disable_timing -from CLK -to Q [get_cells reg1]
set_clock_gating_check -hold 0.1 [get_cells reg1]
set_disable_clock_gating_check [get_cells reg1]
set_max_time_borrow 0.5 [get_cells reg1]
set sdc_file [make_result_file sdc_pin_delete.sdc]
write_sdc -no_timestamp $sdc_file
report_file $sdc_file
# The report path filter names reg1/CLK; deleting reg1 must not free it.
report_checks -from [get_pins reg1/CLK]
delete_instance reg1
write_sdc -no_timestamp $sdc_file
report_file $sdc_file
