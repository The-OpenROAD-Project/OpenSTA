# report_checks -format json with no paths
read_liberty ../examples/asap7_small_ss.lib.gz
read_verilog verilog_attribute.v
link_design counter
create_clock -name clk -period 10
report_checks -path_group clk -format json
