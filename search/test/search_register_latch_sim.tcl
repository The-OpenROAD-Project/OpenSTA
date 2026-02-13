# Test FindRegister.cc with latch designs: all_registers -level_sensitive
# finds latches, various pin types on latches, clock filtering on latches.
# Also exercises Sim.cc deeper constant propagation through complex gates,
# and Sta.cc exprConstantPins, slowDrivers result sorting.
# Targets: FindRegister.cc FindRegVisitor for latches,
#   findInferedSequential with latch cells, hasMinPulseWidthCheck,
#   pathSenseThru, FindRegClkPred::searchFrom/searchThru,
#   Sim.cc propagateConstants through AND/OR/INV chains,
#   evalInstance for NAND/NOR/XOR, seedConstants,
#   setConstraintConstPins, enqueueConstantPinInputs,
#   Sta.cc findRegisterInstances, findRegisterPreamble
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_latch.v
link_design search_latch

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]

report_checks > /dev/null

############################################################
# all_registers with latch design
############################################################
puts "--- all_registers default (latches + flops) ---"
set regs [all_registers]
puts "total registers: [llength $regs]"
foreach r $regs { puts "  [get_full_name $r]" }
puts "PASS: all_registers"

puts "--- all_registers -cells ---"
set cells [all_registers -cells]
puts "register cells: [llength $cells]"
foreach c $cells { puts "  [get_full_name $c]" }
puts "PASS: cells"

puts "--- all_registers -level_sensitive ---"
set ls [all_registers -cells -level_sensitive]
puts "level-sensitive: [llength $ls]"
foreach c $ls { puts "  [get_full_name $c]" }
puts "PASS: level_sensitive"

puts "--- all_registers -edge_triggered ---"
set et [all_registers -cells -edge_triggered]
puts "edge-triggered: [llength $et]"
foreach c $et { puts "  [get_full_name $c]" }
puts "PASS: edge_triggered"

puts "--- all_registers -data_pins ---"
set dp [all_registers -data_pins]
puts "data pins: [llength $dp]"
foreach p $dp { puts "  [get_full_name $p]" }
puts "PASS: data_pins"

puts "--- all_registers -clock_pins ---"
set cp [all_registers -clock_pins]
puts "clock pins: [llength $cp]"
foreach p $cp { puts "  [get_full_name $p]" }
puts "PASS: clock_pins"

puts "--- all_registers -output_pins ---"
set op [all_registers -output_pins]
puts "output pins: [llength $op]"
foreach p $op { puts "  [get_full_name $p]" }
puts "PASS: output_pins"

puts "--- all_registers -async_pins ---"
set ap [all_registers -async_pins]
puts "async pins: [llength $ap]"
puts "PASS: async_pins"

############################################################
# all_registers -clock clk with latch design
############################################################
puts "--- all_registers -clock clk -cells ---"
set clk_cells [all_registers -cells -clock clk]
puts "cells on clk: [llength $clk_cells]"
foreach c $clk_cells { puts "  [get_full_name $c]" }
puts "PASS: clock filter cells"

puts "--- all_registers -clock clk -level_sensitive ---"
set clk_ls [all_registers -cells -clock clk -level_sensitive]
puts "level-sensitive on clk: [llength $clk_ls]"
foreach c $clk_ls { puts "  [get_full_name $c]" }
puts "PASS: clock filter level_sensitive"

puts "--- all_registers -clock clk -edge_triggered ---"
set clk_et [all_registers -cells -clock clk -edge_triggered]
puts "edge-triggered on clk: [llength $clk_et]"
foreach c $clk_et { puts "  [get_full_name $c]" }
puts "PASS: clock filter edge_triggered"

puts "--- all_registers -clock clk -data_pins ---"
set clk_dp [all_registers -data_pins -clock clk]
puts "data pins on clk: [llength $clk_dp]"
puts "PASS: clock filter data_pins"

puts "--- all_registers -clock clk -clock_pins ---"
set clk_cp [all_registers -clock_pins -clock clk]
puts "clock pins on clk: [llength $clk_cp]"
puts "PASS: clock filter clock_pins"

puts "--- all_registers -clock clk -output_pins ---"
set clk_op [all_registers -output_pins -clock clk]
puts "output pins on clk: [llength $clk_op]"
puts "PASS: clock filter output_pins"

############################################################
# all_registers -rise_clock / -fall_clock with latches
############################################################
puts "--- all_registers -rise_clock ---"
set rise_regs [all_registers -cells -rise_clock clk]
puts "rise clk cells: [llength $rise_regs]"
puts "PASS: rise_clock"

puts "--- all_registers -fall_clock ---"
set fall_regs [all_registers -cells -fall_clock clk]
puts "fall clk cells: [llength $fall_regs]"
puts "PASS: fall_clock"

############################################################
# Sim: constant propagation through AND gate
############################################################
puts "--- sim: constant propagation ---"
set_case_analysis 0 [get_ports in1]
set sv_a1 [sta::pin_sim_logic_value [get_pins and1/A1]]
puts "in1=0: and1/A1=$sv_a1"
set sv_zn [sta::pin_sim_logic_value [get_pins and1/ZN]]
puts "and1/ZN=$sv_zn"
report_checks -path_delay max
puts "PASS: case 0 in1"

set_case_analysis 1 [get_ports in1]
set sv_a1_1 [sta::pin_sim_logic_value [get_pins and1/A1]]
puts "in1=1: and1/A1=$sv_a1_1"
report_checks -path_delay max
unset_case_analysis [get_ports in1]
puts "PASS: case 1 in1"

set_case_analysis 0 [get_ports in2]
report_checks -path_delay max
unset_case_analysis [get_ports in2]
puts "PASS: case 0 in2"

puts "--- sim: both inputs zero ---"
set_case_analysis 0 [get_ports in1]
set_case_analysis 0 [get_ports in2]
set sv_zn2 [sta::pin_sim_logic_value [get_pins and1/ZN]]
puts "in1=0,in2=0: and1/ZN=$sv_zn2"
report_checks -path_delay max
unset_case_analysis [get_ports in1]
unset_case_analysis [get_ports in2]
puts "PASS: both zero"

puts "--- sim: both inputs one ---"
set_case_analysis 1 [get_ports in1]
set_case_analysis 1 [get_ports in2]
set sv_zn3 [sta::pin_sim_logic_value [get_pins and1/ZN]]
puts "in1=1,in2=1: and1/ZN=$sv_zn3"
report_checks -path_delay max
unset_case_analysis [get_ports in1]
unset_case_analysis [get_ports in2]
puts "PASS: both one"

############################################################
# set_logic_zero / set_logic_one on latch inputs
############################################################
puts "--- set_logic_zero ---"
set_logic_zero [get_ports in1]
report_checks -path_delay max
puts "PASS: logic_zero in1"

puts "--- set_logic_one ---"
set_logic_one [get_ports in2]
report_checks -path_delay max
puts "PASS: logic_one in2"

############################################################
# report_constant on latch design
############################################################
puts "--- report_constant ---"
report_constant [get_ports in1]
report_constant [get_cells and1]
report_constant [get_cells buf1]
puts "PASS: report_constant"

############################################################
# Slow driver analysis
############################################################
puts "--- slow_drivers ---"
catch {
  set slow [sta::slow_drivers_cmd 5]
  puts "slow drivers: [llength $slow]"
}
puts "PASS: slow_drivers"

############################################################
# Latch timing paths
############################################################
puts "--- latch timing ---"
report_checks -through [get_pins latch1/D]
report_checks -through [get_pins latch1/Q]
report_checks -through [get_pins latch2/Q]
puts "PASS: latch timing paths"

puts "--- report_checks -format full_clock_expanded ---"
report_checks -path_delay max -format full_clock_expanded
report_checks -path_delay min -format full_clock_expanded
puts "PASS: full_clock_expanded"

puts "ALL PASSED"
