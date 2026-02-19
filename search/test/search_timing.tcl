# Full timing analysis flow test
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

# Setup analysis
report_checks -path_delay max

# Hold analysis
report_checks -path_delay min

# Check worst slack
set slack [sta::worst_slack_cmd "max"]
puts "Worst slack: $slack"
if { $slack == "" } {
  puts "FAIL: no slack found"
  exit 1
}
