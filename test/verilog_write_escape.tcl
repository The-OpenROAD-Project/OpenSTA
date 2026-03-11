# Check if "h1\x" and \Y[2:1] are correctly processed from input to output of Verilog
source helpers.tcl
read_liberty gf180mcu_sram.lib.gz
read_liberty asap7_small.lib.gz
read_verilog verilog_write_escape.v
link_design multi_sink
set verilog_file [make_result_file "verilog_write_escape.v"]
write_verilog $verilog_file
report_file $verilog_file
read_verilog $verilog_file
