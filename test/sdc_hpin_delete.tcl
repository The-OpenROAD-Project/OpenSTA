# Clock latency, disables and case analysis on deleted hierarchical pins.
source helpers.tcl
read_liberty asap7_small.lib.gz
read_verilog sdc_hpin_delete.v
link_design sdc_hpin_delete
create_clock -name clk -period 10 [get_ports clk]
set_input_delay 1 -clock clk [get_ports {in1 in2}]
set_output_delay 1 -clock clk [get_ports {out1 out2}]

# Latency on a hierarchical pin applies to the edges through it.
set_clock_latency 20 [get_pins sub1/in]
report_checks -to [get_ports out1] -format end
unset_clock_latency [get_pins sub1/in]
report_checks -to [get_ports out1] -format end
set_clock_latency 20 [get_pins sub1/in]
# Disables through a hierarchical pin are kept as wire edges.
set_disable_timing [get_pins sub1/out]
report_disabled_edges
# Delete the leaf pins of the edges through the hierarchical pins.
delete_instance reg1
delete_instance sub1/u1
report_disabled_edges
# The latency on the hierarchical pin is kept.
set sdc_file [make_result_file sdc_hpin_delete.sdc]
write_sdc -no_timestamp $sdc_file
report_file $sdc_file

# Constants from a deleted pin's case analysis no longer propagate.
set_case_analysis 0 [get_pins buf1/Y]
report_checks -to [get_ports out2] -format end
delete_instance buf1
report_checks -to [get_ports out2] -format end
