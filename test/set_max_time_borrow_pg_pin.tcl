# set_max_time_borrow on a pin or cell after timing has run must skip the
# cell's power/ground pins, which have no graph vertex. Regression for
# upstream 859dbba3 (arrivalInvalid) and the older requiredInvalid gap,
# fixed in 25e089d4/6f37a1d8.
read_liberty asap7_small.lib.gz
read_verilog reg1_asap7.v
link_design top
create_clock -name clk -period 500 {clk1 clk2 clk3}
report_checks
set_max_time_borrow 1 [get_pins r1/CLK]
report_checks
# Cell form with required times already computed.
report_tns
set_max_time_borrow 1 [get_cells r1]
report_tns
puts "no crash"
