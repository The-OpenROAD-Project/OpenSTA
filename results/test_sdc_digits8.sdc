###############################################################################
# Created by write_sdc
###############################################################################
current_design sdc_test2
###############################################################################
# Timing Constraints
###############################################################################
create_clock -name clk1 -period 10.00000000 [get_ports {clk1}]
create_clock -name clk2 -period 20.00000000 [get_ports {clk2}]
set_input_delay 2.00000000 -clock [get_clocks {clk1}] -add_delay [get_ports {in1}]
set_input_delay 2.50000000 -clock [get_clocks {clk1}] -rise -max -add_delay [get_ports {in2}]
set_input_delay 1.00000000 -clock [get_clocks {clk1}] -fall -min -add_delay [get_ports {in2}]
set_input_delay 1.50000000 -clock [get_clocks {clk1}] -clock_fall -add_delay [get_ports {in3}]
set_input_delay 1.79999995 -clock [get_clocks {clk2}] -add_delay [get_ports {in3}]
set_output_delay 3.00000000 -clock [get_clocks {clk1}] -add_delay [get_ports {out1}]
set_output_delay 3.50000000 -clock [get_clocks {clk2}] -rise -max -add_delay [get_ports {out2}]
set_output_delay 1.50000000 -clock [get_clocks {clk2}] -fall -min -add_delay [get_ports {out2}]
###############################################################################
# Environment
###############################################################################
###############################################################################
# Design Rules
###############################################################################
