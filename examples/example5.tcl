# 3 corner with +/- 10% derating example
define_corners wc typ bc
read_liberty -corner wc  example1_slow.lib
read_liberty -corner typ example1_typ.lib
read_liberty -corner bc  example1_fast.lib
read_verilog example1.v
link_design top
set_timing_derate -early 0.9
set_timing_derate -early 1.1
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}
report_checks -path_delay min_max
report_checks -corner typ
