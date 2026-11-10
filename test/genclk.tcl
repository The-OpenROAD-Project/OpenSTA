read_liberty genclk.lib
read_verilog genclk.v
link_design generated_clock
create_clock -name clk -period 10 [get_ports CLK_IN_1] 
create_clock -name clk2 -period 100 [get_ports CLK_IN_2] 

# Should see 9 clocks
puts "Number of clocks: [llength [all_clocks]]"

# Report all clock periods
foreach clk [all_clocks] {
  puts "[get_name $clk] period: [sta::format_time [$clk period] 6]"
}

# Use TCL command to create a generated clock from generated clock
create_generated_clock \
  -name clk_manual \
  -source [get_pins clk_edge_shift/CLK_OUT] \
  -master_clock clk_edge_shift/CLK_OUT \
  -divide_by 2 \
  [get_ports CLK_OUT_2]

# Use TCL command to create a generated clock using edges and shifts
create_generated_clock \
  -name clk_manual_shifts \
  -source [get_ports CLK_IN_2] \
  -master_clock clk2 \
  -edges {1 3 5 6 7} \
  -edge_shift {1 5 -2 3 -8} \
  [get_ports CLK_OUT_3]

# Should see 11 clocks
puts "Number of clocks: [llength [all_clocks]]"

# Use command to validate waveforms
report_clock_properties
