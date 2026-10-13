# set_input_delay -reference_pin / sta::network_changed_non_sdc
# OpenROAD resizer calls sta::network_changed_non_sdc to delete the graph but not SDC.
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
