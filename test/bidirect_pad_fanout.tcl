# bidirect pads with no timing arcs on nets with a real driver
read_liberty asap7_small.lib.gz
read_liberty bidirect_pad_fanout.lib
read_verilog bidirect_pad_fanout.v
link_design top

create_clock -name clk -period 500 [get_pins clk_pad0/PAD]
set_input_delay -clock clk 0 {in1 in2}
set_output_delay -clock clk 0 [all_outputs]

foreach pin {tie/L tie_pad0/PAD sig_drvr/Y sig_pad0/PAD clk_pad0/PAD} {
  puts "$pin [llength [get_timing_edges -from [get_pins $pin]]]"
}

report_checks -from in1 -to sig_q0
report_checks -from ff0/CLK -to ff1/D
