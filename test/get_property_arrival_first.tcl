# Pin arrival properties build the timing graph when nothing has yet.
read_liberty ../examples/nangate45_slow.lib.gz
read_verilog ../examples/example1.v
link_design top
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay 0 -clock clk [get_ports {in1 in2}]
puts "arrival_max_rise: [format %.4f [get_property [get_pins u2/A1] arrival_max_rise]]"
puts "arrival_min_fall: [format %.4f [get_property [get_pins u2/A1] arrival_min_fall]]"
