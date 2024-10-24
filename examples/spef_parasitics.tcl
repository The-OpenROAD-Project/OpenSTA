# delay calc with spef parasitics
read_liberty nangate45_slow.lib.gz
read_verilog example1.v
link_design top
read_spef example1.dspef
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}
report_checks
