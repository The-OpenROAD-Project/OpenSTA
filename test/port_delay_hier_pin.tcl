# port delays on hierarchical pins; delete_clock, delete_instance leaf
read_liberty asap7_small.lib.gz
read_verilog port_delay_hier_pin.v
link_design top
create_clock -name clk -period 1000
# Leaf pins sub1/u1/A, sub1/u2/A.
set_input_delay -clock clk 100 [get_pins sub1/in]
# Leaf pin sub1/u1/Y.
set_output_delay -clock clk 200 [get_pins sub1/out1]
delete_clock clk
create_clock -name clk -period 1000
set_input_delay -clock clk 100 [get_pins sub1/in]
set_output_delay -clock clk 200 [get_pins sub1/out1]
delete_instance sub1/u1
# Leaf pin sub1/u2/Y.
set_output_delay -clock clk 300 [get_pins sub1/out2]
report_checks
