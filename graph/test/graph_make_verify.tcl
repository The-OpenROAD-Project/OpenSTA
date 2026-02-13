# Read liberty and design, make graph, verify
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog graph_test1.v
link_design graph_test1

# Creating the timing graph implicitly tests graph construction
create_clock -name clk -period 10 [get_ports clk]

# report_checks exercises the graph
report_checks -from [get_ports d] -to [get_ports q]
puts "PASS: graph created and timing reported"
