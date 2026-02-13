###############################################################################
# Created by write_sdc
###############################################################################
current_design jpeg_encoder
###############################################################################
# Timing Constraints
###############################################################################
create_clock -name clk -period 5.5000 [get_ports {clk}]
set_input_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {din[0]}]
set_input_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {din[1]}]
set_input_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {din[2]}]
set_input_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {din[3]}]
set_input_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {din[4]}]
set_input_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {din[5]}]
set_input_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {din[6]}]
set_input_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {din[7]}]
set_input_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {dstrb}]
set_input_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {ena}]
set_input_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {qnt_val[0]}]
set_input_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {qnt_val[1]}]
set_input_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {qnt_val[2]}]
set_input_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {qnt_val[3]}]
set_input_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {qnt_val[4]}]
set_input_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {qnt_val[5]}]
set_input_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {qnt_val[6]}]
set_input_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {qnt_val[7]}]
set_input_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {rst}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {amp[0]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {amp[10]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {amp[11]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {amp[1]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {amp[2]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {amp[3]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {amp[4]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {amp[5]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {amp[6]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {amp[7]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {amp[8]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {amp[9]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {douten}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {qnt_cnt[0]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {qnt_cnt[1]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {qnt_cnt[2]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {qnt_cnt[3]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {qnt_cnt[4]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {qnt_cnt[5]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {rlen[0]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {rlen[1]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {rlen[2]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {rlen[3]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {size[0]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {size[1]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {size[2]}]
set_output_delay 1.1000 -clock [get_clocks {clk}] -add_delay [get_ports {size[3]}]
###############################################################################
# Environment
###############################################################################
###############################################################################
# Design Rules
###############################################################################
