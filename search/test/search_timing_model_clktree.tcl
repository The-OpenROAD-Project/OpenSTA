# Test MakeTimingModel.cc with propagated clock: findClkTreeDelays,
# makeClkTreePaths, checkClock, makeGateModelScalar with clock tree.
# Uses search_crpr.v which has real clock buffers.
# Targets: MakeTimingModel.cc findClkTreeDelays, makeClkTreePaths,
#   makeGateModelScalar, checkClock, findClkedOutputPaths with clock tree,
#   makeSetupHoldTimingArcs with propagated clock,
#   findTimingFromInputs with propagated clock,
#   OutputDelays::timingSense, makeInputOutputTimingArcs
source ../../test/helpers.tcl

############################################################
# Part 1: Propagated clock model with clock tree buffers
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_crpr.v
link_design search_crpr

create_clock -name clk -period 10 [get_ports clk]
set_propagated_clock [get_clocks clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]
set_input_transition 0.1 [all_inputs]

# Run timing first
report_checks -path_delay max > /dev/null
report_checks -path_delay min > /dev/null

puts "--- write_timing_model propagated clock ---"
set model1 [make_result_file "model_clktree1.lib"]
write_timing_model -library_name clktree_lib -cell_name clktree_cell $model1
puts "PASS: write model clktree"

puts "--- read back clktree model ---"
read_liberty $model1
puts "PASS: read model clktree"

############################################################
# Part 2: Model with clock latency + uncertainty
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_crpr.v
link_design search_crpr

create_clock -name clk -period 10 [get_ports clk]
set_propagated_clock [get_clocks clk]
set_clock_latency -source 0.5 [get_clocks clk]
set_clock_uncertainty -setup 0.2 [get_clocks clk]
set_clock_uncertainty -hold 0.1 [get_clocks clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]
set_input_transition 0.1 [all_inputs]

report_checks -path_delay max > /dev/null
report_checks -path_delay min > /dev/null

puts "--- write_timing_model with latency + uncertainty ---"
set model2 [make_result_file "model_clktree2.lib"]
write_timing_model -library_name clktree2_lib -cell_name clktree2_cell $model2
puts "PASS: write model clktree with latency"

puts "--- read back clktree2 model ---"
read_liberty $model2
puts "PASS: read model clktree2"

############################################################
# Part 3: Model from latch design with propagated clock
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_latch.v
link_design search_latch

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]
set_input_transition 0.1 [all_inputs]

report_checks -path_delay max > /dev/null
report_checks -path_delay min > /dev/null

puts "--- write_timing_model latch with min/max ---"
set model3 [make_result_file "model_clktree_latch.lib"]
write_timing_model $model3
puts "PASS: write model latch"

puts "--- read back latch model ---"
read_liberty $model3
puts "PASS: read model latch"

############################################################
# Part 4: Model from multicorner design with propagated clock
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_multicorner_analysis.v
link_design search_multicorner_analysis

create_clock -name clk -period 8 [get_ports clk]
set_propagated_clock [get_clocks clk]
set_input_delay -clock clk 0.5 [get_ports in1]
set_input_delay -clock clk 0.5 [get_ports in2]
set_input_delay -clock clk 0.5 [get_ports in3]
set_output_delay -clock clk 1.0 [get_ports out1]
set_output_delay -clock clk 1.0 [get_ports out2]
set_input_transition 0.05 [all_inputs]

report_checks -path_delay max > /dev/null
report_checks -path_delay min > /dev/null

puts "--- write_timing_model multicorner propagated ---"
set model4 [make_result_file "model_clktree_mc.lib"]
write_timing_model -library_name mc_prop_lib -cell_name mc_prop $model4
puts "PASS: write model multicorner propagated"

puts "--- read back multicorner propagated model ---"
read_liberty $model4
puts "PASS: read model multicorner propagated"

############################################################
# Part 5: Model with clock transition
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_path_end_types.v
link_design search_path_end_types

create_clock -name clk -period 10 [get_ports clk]
set_clock_transition 0.15 [get_clocks clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_input_delay -clock clk 0.5 [get_ports rst]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]
set_output_delay -clock clk 2.0 [get_ports out3]
set_input_transition 0.1 [all_inputs]

report_checks -path_delay max > /dev/null

puts "--- write_timing_model with clock transition ---"
set model5 [make_result_file "model_clk_transition.lib"]
write_timing_model -library_name ct_lib -cell_name ct_cell $model5
puts "PASS: write model clock transition"

puts "--- read back clock transition model ---"
read_liberty $model5
puts "PASS: read model clock transition"

puts "ALL PASSED"
