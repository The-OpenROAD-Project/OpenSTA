# Test network traversal and query commands
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog network_test1.v
link_design network_test1

# Create a clock so all_clocks returns something
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_input_delay -clock clk 0 [get_ports in2]
set_output_delay -clock clk 0 [get_ports out1]

puts "--- get_fanin -to output pin -flat ---"
set fanin_pins [get_fanin -to [get_ports out1] -flat]
puts "fanin flat pin count: [llength $fanin_pins]"

puts "--- get_fanin -to output pin -only_cells ---"
set fanin_cells [get_fanin -to [get_ports out1] -only_cells]
puts "fanin cells count: [llength $fanin_cells]"

puts "--- get_fanin -to output pin -startpoints_only ---"
set fanin_start [get_fanin -to [get_ports out1] -startpoints_only]
puts "fanin startpoints count: [llength $fanin_start]"

puts "--- get_fanout -from input pin -flat ---"
set fanout_pins [get_fanout -from [get_ports in1] -flat]
puts "fanout flat pin count: [llength $fanout_pins]"

puts "--- get_fanout -from input pin -only_cells ---"
set fanout_cells [get_fanout -from [get_ports in1] -only_cells]
puts "fanout cells count: [llength $fanout_cells]"

puts "--- get_fanout -from input pin -endpoints_only ---"
set fanout_end [get_fanout -from [get_ports in1] -endpoints_only]
puts "fanout endpoints count: [llength $fanout_end]"

puts "--- get_fanin with -trace_arcs timing ---"
set fanin_timing [get_fanin -to [get_ports out1] -flat -trace_arcs timing]
puts "fanin timing trace count: [llength $fanin_timing]"

puts "--- get_fanin with -trace_arcs enabled ---"
set fanin_enabled [get_fanin -to [get_ports out1] -flat -trace_arcs enabled]
puts "fanin enabled trace count: [llength $fanin_enabled]"

puts "--- get_fanin with -trace_arcs all ---"
set fanin_all [get_fanin -to [get_ports out1] -flat -trace_arcs all]
puts "fanin all trace count: [llength $fanin_all]"

puts "--- get_fanout with -trace_arcs all ---"
set fanout_all [get_fanout -from [get_ports in1] -flat -trace_arcs all]
puts "fanout all trace count: [llength $fanout_all]"

puts "--- get_fanin with -levels ---"
set fanin_lev [get_fanin -to [get_ports out1] -flat -levels 1]
puts "fanin levels=1 count: [llength $fanin_lev]"

puts "--- get_fanin with -pin_levels ---"
set fanin_plev [get_fanin -to [get_ports out1] -flat -pin_levels 2]
puts "fanin pin_levels=2 count: [llength $fanin_plev]"

puts "--- get_fanout with -levels ---"
set fanout_lev [get_fanout -from [get_ports in1] -flat -levels 1]
puts "fanout levels=1 count: [llength $fanout_lev]"

puts "--- get_fanout with -pin_levels ---"
set fanout_plev [get_fanout -from [get_ports in1] -flat -pin_levels 2]
puts "fanout pin_levels=2 count: [llength $fanout_plev]"

puts "--- get_cells -hierarchical ---"
set all_cells [get_cells -hierarchical *]
puts "hierarchical cells count: [llength $all_cells]"

puts "--- get_nets -hierarchical ---"
set all_nets [get_nets -hierarchical *]
puts "hierarchical nets count: [llength $all_nets]"

puts "--- get_pins -hierarchical ---"
set all_pins [get_pins -hierarchical *]
puts "hierarchical pins count: [llength $all_pins]"

puts "--- report_instance buf1 ---"
report_instance buf1

puts "--- report_instance and1 ---"
report_instance and1

puts "--- report_instance reg1 ---"
report_instance reg1

puts "--- report_net n1 ---"
report_net n1

puts "--- report_net n2 ---"
report_net n2

puts "--- get_full_name / get_name for instances ---"
set buf1_inst [get_cells buf1]
puts "buf1 full_name: [get_full_name $buf1_inst]"
puts "buf1 name: [get_name $buf1_inst]"

puts "--- get_full_name / get_name for nets ---"
set n1_net [get_nets n1]
puts "n1 full_name: [get_full_name $n1_net]"
puts "n1 name: [get_name $n1_net]"

puts "--- get_full_name / get_name for pins ---"
set buf1_A [get_pins buf1/A]
puts "buf1/A full_name: [get_full_name $buf1_A]"

puts "--- get_full_name / get_name for ports ---"
set in1_port [get_ports in1]
puts "in1 full_name: [get_full_name $in1_port]"
puts "in1 name: [get_name $in1_port]"

puts "--- all_inputs ---"
set inputs [all_inputs]
puts "all_inputs count: [llength $inputs]"

puts "--- all_outputs ---"
set outputs [all_outputs]
puts "all_outputs count: [llength $outputs]"

puts "--- all_clocks ---"
set clocks [all_clocks]
puts "all_clocks count: [llength $clocks]"

puts "--- get_ports -filter direction == input ---"
set in_ports [get_ports -filter "direction == input"]
puts "input ports count: [llength $in_ports]"

puts "--- get_cells -filter ref_name == BUF_X1 ---"
set buf_cells [get_cells -filter "ref_name == BUF_X1" *]
puts "BUF_X1 cells count: [llength $buf_cells]"

puts "--- get_cells -filter ref_name == AND2_X1 ---"
set and_cells [get_cells -filter "ref_name == AND2_X1" *]
puts "AND2_X1 cells count: [llength $and_cells]"

puts "--- get_cells -filter ref_name == DFF_X1 ---"
set dff_cells [get_cells -filter "ref_name == DFF_X1" *]
puts "DFF_X1 cells count: [llength $dff_cells]"

puts "--- report_checks to verify timing graph ---"
report_checks
