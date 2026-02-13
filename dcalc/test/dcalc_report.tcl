# Test delay calculation
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog dcalc_test1.v
link_design dcalc_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_output_delay -clock clk 0 [get_ports out1]

# Force delay calculation
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: delay calculation completed"

# Report arrival/required
report_checks -path_delay min
puts "PASS: min path delay reported"

report_checks -path_delay max
puts "PASS: max path delay reported"

puts "ALL PASSED"
