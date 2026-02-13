# Test FindRegister.cc - all_registers command with various options
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_latch.v
link_design search_latch

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]

puts "--- all_registers -cells (default) ---"
set cells [all_registers]
puts "All registers: [llength $cells]"
foreach c $cells { puts "  [get_full_name $c]" }

puts "--- all_registers -cells ---"
set cells2 [all_registers -cells]
puts "All register cells: [llength $cells2]"

puts "--- all_registers -data_pins ---"
set dpins [all_registers -data_pins]
puts "Data pins: [llength $dpins]"
foreach p $dpins { puts "  [get_full_name $p]" }

puts "--- all_registers -clock_pins ---"
set ckpins [all_registers -clock_pins]
puts "Clock pins: [llength $ckpins]"
foreach p $ckpins { puts "  [get_full_name $p]" }

puts "--- all_registers -async_pins ---"
set apins [all_registers -async_pins]
puts "Async pins: [llength $apins]"

puts "--- all_registers -output_pins ---"
set opins [all_registers -output_pins]
puts "Output pins: [llength $opins]"
foreach p $opins { puts "  [get_full_name $p]" }

puts "--- all_registers -edge_triggered ---"
set et_cells [all_registers -cells -edge_triggered]
puts "Edge-triggered cells: [llength $et_cells]"
foreach c $et_cells { puts "  [get_full_name $c]" }

puts "--- all_registers -level_sensitive ---"
set ls_cells [all_registers -cells -level_sensitive]
puts "Level-sensitive cells: [llength $ls_cells]"
foreach c $ls_cells { puts "  [get_full_name $c]" }

puts "--- all_registers -clock clk ---"
set clk_cells [all_registers -cells -clock clk]
puts "Cells on clk: [llength $clk_cells]"
foreach c $clk_cells { puts "  [get_full_name $c]" }

puts "--- all_registers -rise_clock clk ---"
set rise_cells [all_registers -cells -rise_clock clk]
puts "Cells on rise clk: [llength $rise_cells]"

puts "--- all_registers -fall_clock clk ---"
set fall_cells [all_registers -cells -fall_clock clk]
puts "Cells on fall clk: [llength $fall_cells]"

puts "--- all_registers -clock clk -data_pins ---"
set clk_dpins [all_registers -data_pins -clock clk]
puts "Data pins on clk: [llength $clk_dpins]"

puts "--- all_registers -clock clk -clock_pins ---"
set clk_ckpins [all_registers -clock_pins -clock clk]
puts "Clock pins on clk: [llength $clk_ckpins]"

puts "--- all_registers -clock clk -output_pins ---"
set clk_opins [all_registers -output_pins -clock clk]
puts "Output pins on clk: [llength $clk_opins]"

puts "--- all_registers -clock clk -edge_triggered -data_pins ---"
set et_dpins [all_registers -data_pins -clock clk -edge_triggered]
puts "Edge-triggered data pins on clk: [llength $et_dpins]"

puts "--- all_registers -clock clk -level_sensitive -data_pins ---"
set ls_dpins [all_registers -data_pins -clock clk -level_sensitive]
puts "Level-sensitive data pins on clk: [llength $ls_dpins]"

puts "ALL PASSED"
