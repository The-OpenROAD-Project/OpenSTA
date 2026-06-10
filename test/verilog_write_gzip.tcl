# Check that write_verilog supports gzip compression natively if .gz extension is provided.
source helpers.tcl
read_liberty asap7_small.lib.gz
read_verilog reg1_asap7.v
link_design top
set verilog_file [make_result_file "verilog_write_gzip.v.gz"]
write_verilog $verilog_file
report_file $verilog_file
