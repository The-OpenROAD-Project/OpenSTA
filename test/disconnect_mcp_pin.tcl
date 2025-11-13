# disconnect/disconnect pin set_multicycle_path
read_liberty asap7_small.lib.gz
read_verilog disconnect_mcp_pin.v
link_design top

create_clock -name clk -period 500 clk
set_input_delay -clock clk 10 data_in[*]

# This SDC defines setup and hold time requirements for data pins
# relative to a clock, typical for a source-synchronous interface.
set_data_check -from clk -to [get_pins u0/A] -setup 10
set_data_check -from clk -to [get_pins u0/A] -hold  10
set_data_check -from clk -to [get_pins u1/A] -setup 10
set_data_check -from clk -to [get_pins u1/A] -hold  10
set_multicycle_path -end   -setup 1 -to [get_pins u0/A]
set_multicycle_path -end   -setup 1 -to [get_pins u1/A]
set_multicycle_path -start -hold  0 -to [get_pins u0/A]
set_multicycle_path -start -hold  0 -to [get_pins u1/A]

report_checks -to u1/A

disconnect_pin data_in[1] u1/A

report_checks -to u1/A

connect_pin data_in[1] u1/A

report_checks -to u1/A

