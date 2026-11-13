# With an ideal clock, required times are not propagated from register
# clock pins back into the clock network, so ckbuf/Z has no slack.
# Register clock pins and clock gating enables (icg/E, cg/A2) keep their
# slack. With a propagated clock, ckbuf/Z has slack again.
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_ideal_clk_requireds.v
link_design search_ideal_clk_requireds
create_clock -name clk -period 0.3 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {in1 in2}]
set_output_delay -clock clk 0 [get_ports {out1 out2}]

puts "--- ideal clock ---"
puts "ckbuf/Z max [get_property [get_pins ckbuf/Z] slack_max] min [get_property [get_pins ckbuf/Z] slack_min]"
puts "r1/CK max [get_property [get_pins r1/CK] slack_max] min [get_property [get_pins r1/CK] slack_min]"
puts "icg/E max [get_property [get_pins icg/E] slack_max] min [get_property [get_pins icg/E] slack_min]"
puts "cg/A2 max [get_property [get_pins cg/A2] slack_max] min [get_property [get_pins cg/A2] slack_min]"

puts "--- ideal clock, after resizing u1 ---"
replace_cell u1 BUF_X4
puts "ckbuf/Z max [get_property [get_pins ckbuf/Z] slack_max] min [get_property [get_pins ckbuf/Z] slack_min]"
puts "icg/E max [get_property [get_pins icg/E] slack_max] min [get_property [get_pins icg/E] slack_min]"

puts "--- propagated clock ---"
set_propagated_clock [all_clocks]
puts "ckbuf/Z max [get_property [get_pins ckbuf/Z] slack_max] min [get_property [get_pins ckbuf/Z] slack_min]"
puts "icg/E max [get_property [get_pins icg/E] slack_max] min [get_property [get_pins icg/E] slack_min]"
