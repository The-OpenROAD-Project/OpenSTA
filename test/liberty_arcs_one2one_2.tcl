# Test one-to-one functionality with mismatched widths where Y width (8) is larger than A width (4)
read_liberty liberty_arcs_one2one_2.lib
read_verilog liberty_arcs_one2one_2.v
link_design liberty_arcs_one2one_2
create_clock -name clk -period 0
set_input_delay -clock clk 0 [all_inputs]
set_output_delay -clock clk 0 [all_outputs]
for {set i 0} {$i < 8} {incr i} {
    puts "report_edges -to partial_wide_inv_cell/Y[$i]"
    report_edges -to partial_wide_inv_cell/Y[$i]
}
