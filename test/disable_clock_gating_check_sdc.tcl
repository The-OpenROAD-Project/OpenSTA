# Persist set_disable_clock_gating_check objects through write_sdc.
source helpers.tcl
read_liberty disable_clock_gating_check.lib
read_verilog disable_clock_gating_check.v
link_design top
create_clock -name clk -period 1.0 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {en1 en2 d1 d2}]

set_disable_clock_gating_check [list \
  [get_lib_cells AND2] \
  [get_cells cg1] \
  [get_pins cg2/B] \
  [get_ports en1]]
set sdc_file [make_result_file disable_clock_gating_check.sdc]
write_sdc -no_timestamp $sdc_file
report_file $sdc_file
