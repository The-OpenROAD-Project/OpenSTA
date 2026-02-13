###############################################################################
# Created by write_sdc
###############################################################################
current_design sdc_test2
###############################################################################
# Timing Constraints
###############################################################################
create_clock -name clk1 -period 10.0000 [get_ports {clk1}]
create_clock -name clk2 -period 20.0000 [get_ports {clk2}]
create_generated_clock -name genclk -source [get_ports {clk1}] -divide_by 2 [get_pins {reg1/Q}]
create_generated_clock -name genclk2 -source [get_ports {clk2}] -multiply_by 3 [get_pins {reg3/Q}]
create_generated_clock -name genclk3 -source [get_ports {clk1}] -edges {1 3 5} [get_pins {reg2/Q}]
set_input_delay 2.0000 -clock [get_clocks {clk1}] -add_delay [get_ports {in1}]
set_output_delay 3.0000 -clock [get_clocks {clk1}] -add_delay [get_ports {out1}]
set_false_path\
    -from [get_clocks {clk1}]\
    -to [get_clocks {clk2}]
###############################################################################
# Environment
###############################################################################
###############################################################################
# Design Rules
###############################################################################
