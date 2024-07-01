# prima reg1 asap7
read_liberty asap7_invbuf.lib.gz
read_liberty asap7_seq.lib.gz
read_liberty asap7_simple.lib.gz
read_verilog reg1_asap7.v
link_design top
create_clock -name clk -period 500 {clk1 clk2 clk3}
set_input_delay -clock clk 1 {in1 in2}
set_input_transition 10 {in1 in2 clk1 clk2 clk3}
set_propagated_clock {clk1 clk2 clk3}
read_spef reg1_asap7.spef
sta::set_delay_calculator prima
report_checks -fields {input_pins slew} -format full_clock
