# A -through moved to a hierarchical pin applies to the edges through it.
source helpers.tcl
read_liberty asap7_small.lib.gz
read_verilog sdc_move_hpin.v
link_design sdc_move_hpin
create_clock -name clk -period 100 [get_ports clk]
set_false_path -through [get_pins h1/out]
sta::move_pin_constraints_cmd [get_pins h1/out] [get_pins h2/out]
report_checks -to r4/D -format end
report_checks -to r2/D -format end
