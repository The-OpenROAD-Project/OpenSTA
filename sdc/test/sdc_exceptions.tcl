# Test exception path SDC commands for code coverage
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

# Setup clocks
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]

# Setup basic delays
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_input_delay -clock clk1 2.0 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]
puts "PASS: basic setup"

############################################################
# set_false_path
############################################################

# False path from port to port
set_false_path -from [get_ports in1] -to [get_ports out1]
puts "PASS: set_false_path -from -to"

# False path with -through
set_false_path -from [get_ports in2] -through [get_pins and1/ZN] -to [get_ports out1]
puts "PASS: set_false_path -from -through -to"

# False path with rise_from/fall_to
set_false_path -rise_from [get_ports in3] -fall_to [get_ports out2]
puts "PASS: set_false_path -rise_from -fall_to"

# False path between clock domains
set_false_path -from [get_clocks clk1] -to [get_clocks clk2]
puts "PASS: set_false_path -from clk -to clk"

# Report to verify false paths
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: report_checks after false_path"

############################################################
# Reset all exceptions and re-add other types
############################################################

unset_path_exceptions -from [get_ports in1] -to [get_ports out1]
puts "PASS: unset_path_exceptions specific"

unset_path_exceptions -from [get_ports in2] -through [get_pins and1/ZN] -to [get_ports out1]
puts "PASS: unset_path_exceptions with -through"

unset_path_exceptions -rise_from [get_ports in3] -fall_to [get_ports out2]
puts "PASS: unset_path_exceptions rise_from/fall_to"

unset_path_exceptions -from [get_clocks clk1] -to [get_clocks clk2]
puts "PASS: unset_path_exceptions clock domain"

############################################################
# set_multicycle_path
############################################################

# Setup multicycle
set_multicycle_path -setup 2 -from [get_clocks clk1] -to [get_clocks clk1]
puts "PASS: set_multicycle_path -setup 2"

# Hold multicycle
set_multicycle_path -hold 1 -from [get_clocks clk1] -to [get_clocks clk1]
puts "PASS: set_multicycle_path -hold 1"

# Multicycle from specific pin
set_multicycle_path -setup 3 -from [get_ports in1] -to [get_ports out1]
puts "PASS: set_multicycle_path -setup 3 pin-to-pin"

report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: report_checks after multicycle"

# Unset multicycle paths
unset_path_exceptions -setup -from [get_clocks clk1] -to [get_clocks clk1]
puts "PASS: unset multicycle setup"

unset_path_exceptions -hold -from [get_clocks clk1] -to [get_clocks clk1]
puts "PASS: unset multicycle hold"

unset_path_exceptions -setup -from [get_ports in1] -to [get_ports out1]
puts "PASS: unset multicycle pin-to-pin"

############################################################
# set_max_delay / set_min_delay
############################################################

set_max_delay -from [get_ports in1] -to [get_ports out1] 8.0
puts "PASS: set_max_delay"

set_min_delay -from [get_ports in1] -to [get_ports out1] 1.0
puts "PASS: set_min_delay"

report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: report_checks after max/min delay"

# Unset the delay constraints
unset_path_exceptions -from [get_ports in1] -to [get_ports out1]
puts "PASS: unset max/min delay paths"

############################################################
# group_path
############################################################

group_path -name group_clk1 -from [get_clocks clk1]
puts "PASS: group_path -name -from"

group_path -name group_io -from [get_ports in1] -to [get_ports out1]
puts "PASS: group_path -name -from -to"

report_checks -path_group group_clk1
puts "PASS: report_checks -path_group group_clk1"

report_checks -path_group group_io
puts "PASS: report_checks -path_group group_io"

# Final report
report_checks
puts "PASS: final report_checks"

puts "ALL PASSED"
