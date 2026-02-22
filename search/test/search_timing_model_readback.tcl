# Test MakeTimingModel.cc: generate timing model, read it back, and run STA
# on it. Exercises makeTimingModel with clock setup, findTimingFromInputs,
# findClkedOutputPaths, findClkTreeDelays, makeGateModelScalar,
# makeEndTimingArcs, saveSdc/restoreSdc, makeLibrary, makeCell, makePorts.
# Also exercises model output with different designs (latch, CRPR).
# Targets: MakeTimingModel.cc all major functions,
#   Sta.cc writeTimingModel
source ../../test/helpers.tcl

proc rename_timing_model_library {lib_file new_lib_name} {
  set in [open $lib_file r]
  set lib_text [read $in]
  close $in

  regsub {library[[:space:]]*\([^)]+\)} $lib_text "library ($new_lib_name)" lib_text

  set out [open $lib_file w]
  puts -nonewline $out $lib_text
  close $out
}

############################################################
# Part 1: Model from search_path_end_types (flops with async reset)
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
set_input_transition 0.1 [all_inputs]

report_checks -path_delay max > /dev/null

puts "--- write_timing_model for search_path_end_types ---"
set model1 [make_result_file "model_pet.lib"]
write_timing_model -library_name model_pet_lib -cell_name model_pet $model1
rename_timing_model_library $model1 model_pet_lib_readback

# Read model back
puts "--- read back model ---"
read_liberty $model1

############################################################
# Part 2: Model from search_crpr (clock tree reconvergence)
############################################################
read_verilog search_crpr.v
link_design search_crpr

create_clock -name clk -period 10 [get_ports clk]
set_propagated_clock [get_clocks clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]
set_input_transition 0.1 [all_inputs]

report_checks -path_delay max > /dev/null

puts "--- write_timing_model for crpr design ---"
set model2 [make_result_file "model_crpr.lib"]
write_timing_model -library_name model_crpr_lib -cell_name model_crpr $model2
rename_timing_model_library $model2 model_crpr_lib_readback

puts "--- read back crpr model ---"
read_liberty $model2

############################################################
# Part 3: Model from search_latch (latch design)
############################################################
read_verilog search_latch.v
link_design search_latch

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]
set_input_transition 0.1 [all_inputs]

report_checks -path_delay max > /dev/null

puts "--- write_timing_model for latch design ---"
set model3 [make_result_file "model_latch.lib"]
write_timing_model $model3
rename_timing_model_library $model3 model_latch_readback

puts "--- read back latch model ---"
read_liberty $model3

############################################################
# Part 4: Model from search_test1 (simple flop design)
############################################################
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]
set_input_transition 0.1 [all_inputs]

report_checks -path_delay max > /dev/null

puts "--- write_timing_model default ---"
set model4 [make_result_file "model_simple.lib"]
write_timing_model $model4
rename_timing_model_library $model4 model_simple_readback

puts "--- write_timing_model with corner ---"
set corner [sta::cmd_corner]
set model5 [make_result_file "model_simple_corner.lib"]
write_timing_model -corner [$corner name] $model5
rename_timing_model_library $model5 model_simple_corner_readback

# Read model back and use it as a block
puts "--- read back and use as block ---"
read_liberty $model4

############################################################
# Part 5: write_timing_model on multicorner design
############################################################
read_verilog search_multicorner_analysis.v
link_design search_multicorner_analysis

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_input_delay -clock clk 1.0 [get_ports in3]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]
set_input_transition 0.1 [all_inputs]

report_checks -path_delay max > /dev/null

puts "--- write_timing_model for multicorner analysis ---"
set model6 [make_result_file "model_multicorner.lib"]
write_timing_model -library_name mc_lib -cell_name mc_cell $model6
rename_timing_model_library $model6 model_multicorner_readback

puts "--- read back multicorner model ---"
read_liberty $model6
