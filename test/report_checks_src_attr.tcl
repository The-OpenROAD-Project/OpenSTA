# report_checks all fields enabled
read_liberty ../examples/sky130hd_tt.lib.gz
read_verilog verilog_attribute.v
link_design counter
create_clock -name clk -period 10 clk
set_input_delay -clock clk 0 [all_inputs -no_clocks]
report_checks -path_group clk -fields {capacitance slew input_pin net src_attr}
