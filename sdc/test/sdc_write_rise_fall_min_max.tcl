# write_sdc writes rise/fall/min/max values that fit none of its shared shapes.
# Four values present, rise != fall and min != max, wrote nothing before.
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

create_clock -name clk1 -period 10 [get_ports clk1]
set_clock_transition 0.03 [get_clocks clk1]
set_clock_transition -rise -max 0.04 [get_clocks clk1]
set_load -pin_load 0.05 [get_ports out1]
set_load -pin_load -rise -min 0.03 [get_ports out1]

set sdc_file [make_result_file sdc_write_rise_fall_min_max.sdc]
write_sdc -no_timestamp $sdc_file
report_file $sdc_file
