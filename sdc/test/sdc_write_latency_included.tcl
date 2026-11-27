# write_sdc keeps -source_latency_included and -network_latency_included
# on set_input_delay and set_output_delay. It wrote neither before.
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

create_clock -name clk1 -period 10 [get_ports clk1]
set_clock_latency -source 2.0 [get_clocks clk1]
set_clock_latency 0.5 [get_clocks clk1]
set_input_delay 1.0 -clock clk1 -source_latency_included [get_ports in1]
set_input_delay 1.0 -clock clk1 -network_latency_included [get_ports in2]
set_input_delay 1.0 -clock clk1 -clock_fall -source_latency_included \
  -network_latency_included [get_ports in3]
set_output_delay 1.0 -clock clk1 -source_latency_included \
  -network_latency_included [get_ports out1]
set_output_delay 1.0 -clock clk1 [get_ports out2]

set sdc_file [make_result_file sdc_write_latency_included.sdc]
write_sdc -no_timestamp $sdc_file
report_file $sdc_file
