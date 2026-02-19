# Test graph traversal: get_fanin, get_fanout, edges
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

# Helper to print a collection of objects
proc print_names { objs } {
  set names {}
  foreach obj $objs {
    lappend names [get_full_name $obj]
  }
  return [lsort $names]
}

puts "--- get_fanin -to reg1/D -flat ---"
set fanin_pins [get_fanin -to [get_pins reg1/D] -flat]
puts "Fanin pins: [print_names $fanin_pins]"

puts "--- get_fanin -to reg1/D -only_cells ---"
set fanin_cells [get_fanin -to [get_pins reg1/D] -only_cells]
puts "Fanin cells: [print_names $fanin_cells]"

puts "--- get_fanin -to reg1/D -startpoints_only ---"
set fanin_start [get_fanin -to [get_pins reg1/D] -startpoints_only]
puts "Fanin startpoints: [print_names $fanin_start]"

puts "--- get_fanin -to out1 -flat ---"
set fanin_out [get_fanin -to [get_ports out1] -flat]
puts "Fanin to out1: [print_names $fanin_out]"

puts "--- get_fanin -to out1 -only_cells ---"
set fanin_out_cells [get_fanin -to [get_ports out1] -only_cells]
puts "Fanin cells to out1: [print_names $fanin_out_cells]"

puts "--- get_fanin -to out1 -startpoints_only ---"
set fanin_out_start [get_fanin -to [get_ports out1] -startpoints_only]
puts "Fanin startpoints to out1: [print_names $fanin_out_start]"

puts "--- get_fanin -levels ---"
set fanin_lev [get_fanin -to [get_pins reg1/D] -flat -levels 1]
puts "Fanin 1 level: [print_names $fanin_lev]"

puts "--- get_fanin -pin_levels ---"
set fanin_plev [get_fanin -to [get_pins reg1/D] -flat -pin_levels 2]
puts "Fanin pin_levels 2: [print_names $fanin_plev]"

puts "--- get_fanin -trace_arcs timing ---"
set fanin_timing [get_fanin -to [get_pins reg1/D] -flat -trace_arcs timing]
puts "Fanin trace timing: [print_names $fanin_timing]"

puts "--- get_fanin -trace_arcs enabled ---"
set fanin_enabled [get_fanin -to [get_pins reg1/D] -flat -trace_arcs enabled]
puts "Fanin trace enabled: [print_names $fanin_enabled]"

puts "--- get_fanin -trace_arcs all ---"
set fanin_all [get_fanin -to [get_pins reg1/D] -flat -trace_arcs all]
puts "Fanin trace all: [print_names $fanin_all]"

puts "--- get_fanout -from in1 -flat ---"
set fanout_pins [get_fanout -from [get_ports in1] -flat]
puts "Fanout pins: [print_names $fanout_pins]"

puts "--- get_fanout -from in1 -only_cells ---"
set fanout_cells [get_fanout -from [get_ports in1] -only_cells]
puts "Fanout cells: [print_names $fanout_cells]"

puts "--- get_fanout -from in1 -endpoints_only ---"
set fanout_end [get_fanout -from [get_ports in1] -endpoints_only]
puts "Fanout endpoints: [print_names $fanout_end]"

puts "--- get_fanout -from reg1/Q -flat ---"
set fanout_q [get_fanout -from [get_pins reg1/Q] -flat]
puts "Fanout from reg1/Q: [print_names $fanout_q]"

puts "--- get_fanout -from reg1/Q -only_cells ---"
set fanout_q_cells [get_fanout -from [get_pins reg1/Q] -only_cells]
puts "Fanout cells from reg1/Q: [print_names $fanout_q_cells]"

puts "--- get_fanout -from reg1/Q -endpoints_only ---"
set fanout_q_end [get_fanout -from [get_pins reg1/Q] -endpoints_only]
puts "Fanout endpoints from reg1/Q: [print_names $fanout_q_end]"

puts "--- get_fanout -levels ---"
set fanout_lev [get_fanout -from [get_ports in1] -flat -levels 1]
puts "Fanout 1 level: [print_names $fanout_lev]"

puts "--- get_fanout -pin_levels ---"
set fanout_plev [get_fanout -from [get_ports in1] -flat -pin_levels 2]
puts "Fanout pin_levels 2: [print_names $fanout_plev]"

puts "--- get_fanout -trace_arcs timing ---"
set fanout_timing [get_fanout -from [get_ports in1] -flat -trace_arcs timing]
puts "Fanout trace timing: [print_names $fanout_timing]"

puts "--- get_fanout -trace_arcs enabled ---"
set fanout_enabled [get_fanout -from [get_ports in1] -flat -trace_arcs enabled]
puts "Fanout trace enabled: [print_names $fanout_enabled]"

puts "--- get_fanout -trace_arcs all ---"
set fanout_all [get_fanout -from [get_ports in1] -flat -trace_arcs all]
puts "Fanout trace all: [print_names $fanout_all]"

puts "--- get_timing_edges -from -to ---"
set edges_ft [get_timing_edges -from [get_pins and1/A1] -to [get_pins and1/ZN]]
puts "Edges from/to: [llength $edges_ft] edges"

puts "--- get_timing_edges -from ---"
set edges_f [get_timing_edges -from [get_pins buf1/A]]
puts "Edges from buf1/A: [llength $edges_f] edges"

puts "--- get_timing_edges -to ---"
set edges_t [get_timing_edges -to [get_pins buf1/Z]]
puts "Edges to buf1/Z: [llength $edges_t] edges"

puts "--- get_timing_edges -of_objects instance ---"
set edges_inst [get_timing_edges -of_objects [get_cells and1]]
puts "Edges of and1: [llength $edges_inst] edges"

puts "--- get_timing_edges -of_objects lib_cell ---"
set edges_lib [get_timing_edges -of_objects [get_lib_cells Nangate45_typ/AND2_X1]]
puts "Edges of AND2_X1 lib cell: [llength $edges_lib] edges"

puts "--- report_edges -from ---"
report_edges -from [get_pins and1/A1]

puts "--- report_edges -to ---"
report_edges -to [get_pins and1/ZN]

puts "--- report_edges -from -to ---"
report_edges -from [get_pins and1/A1] -to [get_pins and1/ZN]

puts "--- report_edges -from buf1/A ---"
report_edges -from [get_pins buf1/A]

puts "--- report_edges -from reg1/CK ---"
report_edges -from [get_pins reg1/CK]

puts "--- report_path for a pin ---"
report_path -max [get_pins reg1/D] rise

puts "--- report_path -min ---"
report_path -min [get_pins reg1/D] rise
