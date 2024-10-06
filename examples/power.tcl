# report_power gcd
read_liberty sky130hd_tt.lib.gz
read_verilog gcd_sky130hd.v
link_design gcd

read_sdc gcd_sky130hd.sdc
set_propagated_clock clk
read_spef gcd_sky130hd.spef
set_power_activity -input -activity .1
set_power_activity -input_port reset -activity 0
report_power
