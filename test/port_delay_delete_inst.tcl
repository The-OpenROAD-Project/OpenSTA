# set_input_delay inst/pin; delete_instance inst
read_liberty asap7_small.lib.gz
read_verilog reg1_asap7.v
link_design top
create_clock -name clk -period 1000 {clk1 clk2 clk3}
# Port delays on the pins of an instance that is deleted below.
set_output_delay -clock clk 130 [get_pins u1/Y]
set_input_delay -clock clk 120 [get_pins u1/A]
delete_instance u1
report_checks
