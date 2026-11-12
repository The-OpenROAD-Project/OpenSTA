# read_spef of a net that has parasitics adds to them, or replaces them with -replace.
read_liberty asap7_small.lib.gz
read_verilog reg1_asap7.v
link_design top
create_clock -name clk -period 500 {clk1 clk2 clk3}
read_spef reg1_asap7.spef
report_checks -through u1/Y -fields {cap} -digits 2
# Adds to the u1z parasitics read above.
read_spef reg1_asap7_u1z.spef
report_checks -through u1/Y -fields {cap} -digits 2
# Replaces the u1z parasitics.
read_spef -replace reg1_asap7_u1z.spef
report_checks -through u1/Y -fields {cap} -digits 2
