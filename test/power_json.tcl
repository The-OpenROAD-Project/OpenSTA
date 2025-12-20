# report_power reg1_asap7
set sta_report_default_digits 4
read_liberty ../examples/asap7_small.lib.gz
read_verilog ../examples/reg1_asap7.v
link_design top

create_clock -name clk1 -period 500 clk1
create_clock -name clk2 -period 500 clk2
create_clock -name clk3 -period 500 clk3

set_input_delay -clock clk1 1 {in1 in2}
set_input_delay -clock clk2 1 {in1 in2}
set_input_delay -clock clk3 1 {in1 in2}
set_input_transition 10 {in1 in2 clk1 clk2 clk3}

set_propagated_clock {clk1 clk2 clk3}

read_spef ../examples/reg1_asap7.spef

set_power_activity -input -activity .1
set_power_activity -input_port reset -activity 0

report_power -format json
report_power -format json -instances "[get_cells -filter "name=~clkbuf*"]"