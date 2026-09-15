# dmp_ceff_two_pole with read_spef -reduce
# The reduce hook must produce PiPoleResidue parasitics for the two pole
# delay calculator. If it produces PiElmore instead, loadDelaySlew ignores
# the parasitic and wire delays are zero.
read_liberty asap7_small.lib.gz
read_verilog reg1_asap7.v
link_design top
create_clock -name clk -period 500 {clk1 clk2 clk3}
set_delay_calculator dmp_ceff_two_pole
read_spef -reduce reg1_asap7.spef
report_checks -fields {input_pins}
