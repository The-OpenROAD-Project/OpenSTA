# Disable inferred clock gating checks on a top-level enable port.
read_liberty disable_clock_gating_check.lib
read_verilog disable_clock_gating_check.v
link_design top
create_clock -name clk -period 1.0 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {en1 en2 d1 d2}]

puts "before cg1"
report_checks -to cg1/B -path_delay max -format end
puts "before cg2"
report_checks -to cg2/B -path_delay max -format end

puts "disable en1"
set_disable_clock_gating_check [get_ports en1]
puts "cg1"
report_checks -to cg1/B -path_delay max -format end
puts "cg2"
report_checks -to cg2/B -path_delay max -format end

puts "unset en1"
unset_disable_clock_gating_check [get_ports en1]
puts "cg1"
report_checks -to cg1/B -path_delay max -format end
puts "cg2"
report_checks -to cg2/B -path_delay max -format end
