# report_power reg1_asap7
read_liberty asap7_small.lib.gz
read_verilog reg1_asap7.v
link_design top
create_clock -name clk -period 500 {clk1 clk2 clk3}
set_propagated_clock {clk1 clk2 clk3}
report_power -format json
report_power -format json -instances {u1 u2}
