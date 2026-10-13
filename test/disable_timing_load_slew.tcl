# Disabled driver arcs must not leave load slews at the min/max init
# value (+/-INF). Regression for upstream 9ee64aea, fixed in 9d588bc9/99ffdffd.
read_liberty asap7_small.lib.gz
read_verilog reg1_asap7.v
link_design top
create_clock -name clk -period 500 {clk1 clk2 clk3}
set_input_delay -clock clk 1 {in1 in2}

puts "--- before set_disable_timing"
report_checks -to r3/D -fields {slew} -digits 3

set_disable_timing [get_cells u1]

puts "--- after set_disable_timing \[get_cells u1\]"
report_checks -to r3/D -fields {slew} -digits 3

# u2/B is the load of the disabled driver u1/Y.
report_slews [get_pins u2/B]
