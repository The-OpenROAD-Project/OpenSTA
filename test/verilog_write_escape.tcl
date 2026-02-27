# Check if "h1\x" and \Y[2:1] are correctly processed from input to output of Verilog
read_liberty gf180mcu_sram.lib.gz
read_liberty asap7_small.lib.gz
read_verilog verilog_write_escape.v
link_design multi_sink
set output_file "verilog_write_escape_out.v"
write_verilog $output_file
set fp [open $output_file r]
while {[gets $fp line] >= 0} {
    puts $line
}
close $fp
read_verilog $output_file
file delete $output_file
