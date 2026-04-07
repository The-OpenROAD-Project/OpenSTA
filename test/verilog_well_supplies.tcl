# Check that write_verilog excludes well pins along with power/ground pins.
source helpers.tcl
read_liberty ../examples/sky130hd_tt.lib.gz
read_verilog verilog_well_supplies.v
link_design top
set verilog_file [make_result_file "verilog_well_supplies.v"]
write_verilog $verilog_file
report_file $verilog_file

set verilog_pwr_file [make_result_file "verilog_well_supplies_pwr.v"]
write_verilog -include_pwr_gnd $verilog_pwr_file
report_file $verilog_pwr_file
