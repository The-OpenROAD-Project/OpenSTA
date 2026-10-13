# set_input_delay -reference_pin must survive a graph rebuild that keeps SDC
# (resizer-like: sta::network_changed_non_sdc deletes the graph but not the
# constraints). The ref_pin->input graph edge is owned by the graph; its
# existence flag (PortDelay::ref_pin_edges_exist_) must be reset on graph
# teardown so ensureInputDelayRefPinEdges() rebuilds the edge. Otherwise in1
# loses its arrival seeding and reports "No paths found" after the rebuild.
# in2 is a plain input delay (no ref_pin) used as a control.
read_liberty asap7_small.lib.gz
read_verilog reg1_asap7.v
link_design top

create_clock -name clk -period 500 {clk1 clk2 clk3}
set_input_delay -clock clk 100 -reference_pin r2/CLK [get_ports in1]
set_input_delay -clock clk 100 [get_ports in2]
set_input_transition 10 {in1 in2 clk1 clk2 clk3}
set_output_delay -clock clk 1 [get_ports out]

puts "=== before rebuild: in1 (ref_pin) ==="
report_checks -from in1 -path_delay max
puts "=== before rebuild: in2 (control) ==="
report_checks -from in2 -path_delay max

sta::network_changed_non_sdc

puts "=== after rebuild: in1 (ref_pin) ==="
report_checks -from in1 -path_delay max
puts "=== after rebuild: in2 (control) ==="
report_checks -from in2 -path_delay max
