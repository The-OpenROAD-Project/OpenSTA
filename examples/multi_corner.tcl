# 3 corners with +/- 10% derating example
define_corners ss tt ff
read_liberty -corner ss nangate45_slow.lib.gz
read_liberty -corner tt nangate45_typ.lib.gz
read_liberty -corner ff nangate45_fast.lib.gz
read_verilog example1.v
link_design top
set_timing_derate -early 0.9
set_timing_derate -late  1.1
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}
# report all corners
report_checks -path_delay min_max
# report typical corner
report_checks -corner tt
