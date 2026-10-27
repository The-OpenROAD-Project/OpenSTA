# create_clock hierarchical/leaf pin; delete_instance clock driver
read_liberty asap7_small.lib.gz
read_verilog clock_delete_inst.v
link_design top
# Clock on a hierarchical pin is sourced at its leaf driver u1/b1/Y.
create_clock -name hclk -period 500 [get_pins u1/clk_out]
# Clock directly on a leaf pin.
create_clock -name lclk -period 1000 [get_pins b2/Y]
set_output_delay -clock hclk 10 out1
set_output_delay -clock lclk 10 out2
report_checks -group_path_count 2
# Delete the clock driver pins.
delete_instance u1/b1
delete_instance b2
report_checks -group_path_count 2
