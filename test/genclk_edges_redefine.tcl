read_liberty genclk_edges_redefine.lib
read_verilog genclk_edges_redefine.v
link_design generated_clock_edges_redefine

# Create a master clock, triggering liberty-defined generated clocks
# with edges. This exercises makeLibertyGeneratedClocks which passes
# edges/edge_shifts pointers from LibertyGenClk to Clock.
create_clock -name clk -period 10 [get_ports CLK_IN]

puts "After first create_clock:"
report_clock_properties

# Redefine the same clock. This destroys the old generated Clock objects
# (freeing edges_), then creates new ones reusing the same LibertyGenClk
# edges pointer. Without the deep-copy fix in makeLibertyGeneratedClocks,
# this triggers a double-free / use-after-free.
create_clock -name clk -period 20 [get_ports CLK_IN]

puts "After second create_clock:"
report_clock_properties

# Third redefinition to further stress the ownership.
create_clock -name clk -period 5 [get_ports CLK_IN]

puts "After third create_clock:"
report_clock_properties

puts "Test passed."
