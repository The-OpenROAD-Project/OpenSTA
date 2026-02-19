# Test FindRegister.cc with all filter combinations:
# edge_triggered + latches, rise_clock/fall_clock per pin type,
# level_sensitive-only queries, inferred sequentials, and visitSequential paths.
# Uses search_latch.v (has latches) and search_path_end_types.v (has async reset DFFs).
# Targets: FindRegister.cc FindRegVisitor::visitRegs (all branches),
#   findSequential (clk_rf matching with rise/fall),
#   findInferedSequential (latchEnToQ matching),
#   FindRegDataPins::matchPin, FindRegClkPins::matchPin,
#   FindRegAsyncPins::matchPin, FindRegOutputPins::matchPin,
#   FindRegOutputPins::visitSequential, visitOutput,
#   FindRegPins::visitExpr, visitReg (infered path)
source ../../test/helpers.tcl

############################################################
# Part 1: Latch design - level sensitive queries
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_latch.v
link_design search_latch

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]

report_checks > /dev/null

puts "--- latch: all_registers default ---"
set regs [all_registers]
puts "all registers: [llength $regs]"
foreach r $regs { puts "  [get_full_name $r]" }

puts "--- latch: all_registers -cells ---"
set reg_cells [all_registers -cells]
puts "cells: [llength $reg_cells]"
foreach c $reg_cells { puts "  [get_full_name $c]" }

puts "--- latch: all_registers -level_sensitive ---"
set ls [all_registers -cells -level_sensitive]
puts "level_sensitive: [llength $ls]"
foreach c $ls { puts "  [get_full_name $c]" }

puts "--- latch: all_registers -edge_triggered ---"
set et [all_registers -cells -edge_triggered]
puts "edge_triggered: [llength $et]"
foreach c $et { puts "  [get_full_name $c]" }

puts "--- latch: all_registers -level_sensitive -data_pins ---"
set ls_dp [all_registers -data_pins -level_sensitive]
puts "level_sensitive data_pins: [llength $ls_dp]"
foreach p $ls_dp { puts "  [get_full_name $p]" }

puts "--- latch: all_registers -level_sensitive -clock_pins ---"
set ls_ck [all_registers -clock_pins -level_sensitive]
puts "level_sensitive clock_pins: [llength $ls_ck]"
foreach p $ls_ck { puts "  [get_full_name $p]" }

puts "--- latch: all_registers -level_sensitive -output_pins ---"
set ls_op [all_registers -output_pins -level_sensitive]
puts "level_sensitive output_pins: [llength $ls_op]"
foreach p $ls_op { puts "  [get_full_name $p]" }

puts "--- latch: all_registers -edge_triggered -data_pins ---"
set et_dp [all_registers -data_pins -edge_triggered]
puts "edge_triggered data_pins: [llength $et_dp]"
foreach p $et_dp { puts "  [get_full_name $p]" }

puts "--- latch: all_registers -edge_triggered -clock_pins ---"
set et_ck [all_registers -clock_pins -edge_triggered]
puts "edge_triggered clock_pins: [llength $et_ck]"
foreach p $et_ck { puts "  [get_full_name $p]" }

puts "--- latch: all_registers -edge_triggered -output_pins ---"
set et_op [all_registers -output_pins -edge_triggered]
puts "edge_triggered output_pins: [llength $et_op]"
foreach p $et_op { puts "  [get_full_name $p]" }

############################################################
# Rise/fall clock filtering on latches
############################################################
puts "--- latch: all_registers -rise_clock ---"
set rise_ls [all_registers -cells -rise_clock clk]
puts "rise_clock cells: [llength $rise_ls]"
foreach c $rise_ls { puts "  [get_full_name $c]" }

puts "--- latch: all_registers -fall_clock ---"
set fall_ls [all_registers -cells -fall_clock clk]
puts "fall_clock cells: [llength $fall_ls]"
foreach c $fall_ls { puts "  [get_full_name $c]" }

puts "--- latch: all_registers -rise_clock -level_sensitive ---"
set rise_ls_ls [all_registers -cells -rise_clock clk -level_sensitive]
puts "rise level_sensitive: [llength $rise_ls_ls]"

puts "--- latch: all_registers -fall_clock -level_sensitive ---"
set fall_ls_ls [all_registers -cells -fall_clock clk -level_sensitive]
puts "fall level_sensitive: [llength $fall_ls_ls]"

puts "--- latch: all_registers -rise_clock -edge_triggered ---"
set rise_et [all_registers -cells -rise_clock clk -edge_triggered]
puts "rise edge_triggered: [llength $rise_et]"

puts "--- latch: all_registers -fall_clock -edge_triggered ---"
set fall_et [all_registers -cells -fall_clock clk -edge_triggered]
puts "fall edge_triggered: [llength $fall_et]"

puts "--- latch: all_registers -rise_clock -data_pins ---"
set rise_dp [all_registers -data_pins -rise_clock clk]
puts "rise data_pins: [llength $rise_dp]"

puts "--- latch: all_registers -fall_clock -data_pins ---"
set fall_dp [all_registers -data_pins -fall_clock clk]
puts "fall data_pins: [llength $fall_dp]"

puts "--- latch: all_registers -rise_clock -clock_pins ---"
set rise_ck [all_registers -clock_pins -rise_clock clk]
puts "rise clock_pins: [llength $rise_ck]"

puts "--- latch: all_registers -fall_clock -clock_pins ---"
set fall_ck [all_registers -clock_pins -fall_clock clk]
puts "fall clock_pins: [llength $fall_ck]"

puts "--- latch: all_registers -rise_clock -output_pins ---"
set rise_op [all_registers -output_pins -rise_clock clk]
puts "rise output_pins: [llength $rise_op]"

puts "--- latch: all_registers -fall_clock -output_pins ---"
set fall_op [all_registers -output_pins -fall_clock clk]
puts "fall output_pins: [llength $fall_op]"

############################################################
# Part 2: Async reset DFF design - async pin queries
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_path_end_types.v
link_design search_path_end_types

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_input_delay -clock clk 0.5 [get_ports rst]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]
set_output_delay -clock clk 2.0 [get_ports out3]

report_checks > /dev/null

puts "--- async: all_registers -async_pins ---"
set apins [all_registers -async_pins]
puts "async pins: [llength $apins]"
foreach p $apins { puts "  [get_full_name $p]" }

puts "--- async: all_registers -async_pins -clock clk ---"
set apins_c [all_registers -async_pins -clock clk]
puts "async pins clk: [llength $apins_c]"
foreach p $apins_c { puts "  [get_full_name $p]" }

puts "--- async: all_registers -async_pins -edge_triggered ---"
set apins_et [all_registers -async_pins -edge_triggered]
puts "async pins edge_triggered: [llength $apins_et]"

puts "--- async: all_registers -output_pins ---"
set opins [all_registers -output_pins]
puts "output pins: [llength $opins]"
foreach p $opins { puts "  [get_full_name $p]" }

puts "--- async: all_registers -output_pins -edge_triggered ---"
set opins_et [all_registers -output_pins -edge_triggered]
puts "output pins edge_triggered: [llength $opins_et]"
foreach p $opins_et { puts "  [get_full_name $p]" }

puts "--- async: all_registers -output_pins -clock clk ---"
set opins_c [all_registers -output_pins -clock clk]
puts "output pins clk: [llength $opins_c]"
foreach p $opins_c { puts "  [get_full_name $p]" }

puts "--- async: all_registers -rise_clock -async_pins ---"
set rise_ap [all_registers -async_pins -rise_clock clk]
puts "rise async_pins: [llength $rise_ap]"

puts "--- async: all_registers -fall_clock -async_pins ---"
set fall_ap [all_registers -async_pins -fall_clock clk]
puts "fall async_pins: [llength $fall_ap]"

puts "--- async: all_registers -rise_clock -output_pins ---"
set rise_op2 [all_registers -output_pins -rise_clock clk]
puts "rise output_pins: [llength $rise_op2]"

puts "--- async: all_registers -fall_clock -output_pins ---"
set fall_op2 [all_registers -output_pins -fall_clock clk]
puts "fall output_pins: [llength $fall_op2]"
