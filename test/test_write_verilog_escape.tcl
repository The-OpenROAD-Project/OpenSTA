# Check if "h1\x" and \Y[2:1] are correctly processed from input to output of Verilog


read_liberty gf180mcu_sram.lib.gz
read_liberty asap7_small.lib.gz

read_verilog test_write_verilog_escape.v

link_design multi_sink

write_verilog test_write_verilog_escape_out.v
