# report_checks -format json with no paths
read_liberty ../examples/sky130hd_tt.lib.gz
read_verilog verilog_attribute.v
link_design counter
create_clock -name clk -period 10
report_checks -path_group clk -format json
