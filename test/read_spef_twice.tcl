# read_spef after timing has run: the reduced models made from the
# earlier parasitics are stale and must not be used for the next report.
read_liberty asap7_small.lib.gz
read_verilog reg1_asap7.v
link_design top
create_clock -name clk -period 500 {clk1 clk2 clk3}
read_spef reg1_asap7.spef
report_checks -through u1/Y -fields {cap} -digits 2
read_spef reg1_asap7_u1z.spef
report_checks -through u1/Y -fields {cap} -digits 2
