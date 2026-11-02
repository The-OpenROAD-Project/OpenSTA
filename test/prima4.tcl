# prima fast clock edge, light load must cross slew thresholds (no INF slews)
read_liberty asap7_seq.lib.gz
read_verilog prima4.v
link_design top
create_clock -name clk -period 500 clk
set_clock_transition 10 clk
set_output_delay -clock clk 0 q
read_spef prima4.spef
sta::set_delay_calculator prima
report_checks -fields {slew} -digits 3
