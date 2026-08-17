read_liberty non_seq_timing.lib
read_liberty asap7_seq.lib.gz

read_verilog non_seq_timing.v
link_design test

create_clock -name clk -period 10 [get_ports *clk*]

report_checks
