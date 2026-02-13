###############################################################################
# Created by write_sdc
###############################################################################
current_design sdc_test2
###############################################################################
# Timing Constraints
###############################################################################
create_clock -name clk1 -period 10.0000 [get_ports {clk1}]
create_clock -name clk2 -period 20.0000 [get_ports {clk2}]
set_input_delay 2.0000 -clock [get_clocks {clk1}] -add_delay [get_ports {in1}]
set_input_delay 2.5000 -clock [get_clocks {clk1}] -rise -max -add_delay [get_ports {in2}]
set_input_delay 1.0000 -clock [get_clocks {clk1}] -fall -min -add_delay [get_ports {in2}]
set_input_delay 1.5000 -clock [get_clocks {clk1}] -clock_fall -add_delay [get_ports {in3}]
set_input_delay 1.8000 -clock [get_clocks {clk2}] -add_delay [get_ports {in3}]
set_output_delay 3.0000 -clock [get_clocks {clk1}] -add_delay [get_ports {out1}]
set_output_delay 3.5000 -clock [get_clocks {clk2}] -rise -max -add_delay [get_ports {out2}]
set_output_delay 1.5000 -clock [get_clocks {clk2}] -fall -min -add_delay [get_ports {out2}]
###############################################################################
# Environment
###############################################################################
###############################################################################
# Design Rules
###############################################################################
