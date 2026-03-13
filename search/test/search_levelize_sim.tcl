# Test Levelize.cc and Sim.cc code paths
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

# Force timing first
report_checks > /dev/null

puts "--- levelize ---"
sta::levelize

puts "--- report_loops ---"
sta::report_loops

puts "--- Sim logic values ---"
set sv_and [sta::pin_sim_logic_value [get_pins and1/ZN]]
set sv_buf [sta::pin_sim_logic_value [get_pins buf1/Z]]
set sv_reg_d [sta::pin_sim_logic_value [get_pins reg1/D]]
set sv_reg_q [sta::pin_sim_logic_value [get_pins reg1/Q]]
set sv_and_a1 [sta::pin_sim_logic_value [get_pins and1/A1]]
set sv_and_a2 [sta::pin_sim_logic_value [get_pins and1/A2]]
puts "and1/A1=$sv_and_a1 and1/A2=$sv_and_a2 and1/ZN=$sv_and buf1/Z=$sv_buf reg1/D=$sv_reg_d reg1/Q=$sv_reg_q"

puts "--- Case analysis effects on simulation ---"
set_case_analysis 0 [get_ports in1]
set sv_and_0 [sta::pin_sim_logic_value [get_pins and1/ZN]]
puts "in1=0: and1/ZN=$sv_and_0"
unset_case_analysis [get_ports in1]

set_case_analysis 1 [get_ports in1]
set sv_a1_1 [sta::pin_sim_logic_value [get_pins and1/A1]]
puts "in1=1: and1/A1=$sv_a1_1"
unset_case_analysis [get_ports in1]

set_case_analysis 0 [get_ports in2]
set sv_and_02 [sta::pin_sim_logic_value [get_pins and1/ZN]]
puts "in2=0: and1/ZN=$sv_and_02"

# With both inputs set
set_case_analysis 1 [get_ports in1]
set sv_and_10 [sta::pin_sim_logic_value [get_pins and1/ZN]]
puts "in1=1,in2=0: and1/ZN=$sv_and_10"
unset_case_analysis [get_ports in1]
unset_case_analysis [get_ports in2]

set_case_analysis 1 [get_ports in1]
set_case_analysis 1 [get_ports in2]
set sv_and_11 [sta::pin_sim_logic_value [get_pins and1/ZN]]
puts "in1=1,in2=1: and1/ZN=$sv_and_11"
unset_case_analysis [get_ports in1]
unset_case_analysis [get_ports in2]

puts "--- report_constant after case analysis ---"
set_case_analysis 0 [get_ports in1]
report_checks -path_delay max
report_constant [get_ports in1]
report_constant [get_cells and1]
unset_case_analysis [get_ports in1]

puts "--- disable_timing and re-levelize ---"
set_disable_timing [get_cells buf1]
report_checks -path_delay max
sta::levelize
unset_disable_timing [get_cells buf1]
report_checks -path_delay max
sta::levelize

puts "--- Timing after set_disable_timing on lib cell ---"
set_disable_timing -from A -to Z [get_lib_cells Nangate45_typ/BUF_X1]
report_checks -path_delay max
report_disabled_edges
unset_disable_timing -from A -to Z [get_lib_cells Nangate45_typ/BUF_X1]
report_checks -path_delay max

puts "--- Check timing after clear/rerun ---"
sta::find_timing_cmd 1
report_checks -path_delay max

puts "--- find_timing not full ---"
sta::arrivals_invalid
sta::find_timing_cmd 0
report_checks -path_delay max

puts "--- Preset/clear arcs ---"
sta::set_preset_clr_arcs_enabled 1
report_checks -path_delay max
sta::set_preset_clr_arcs_enabled 0
report_checks -path_delay max

puts "--- Conditional default arcs ---"
sta::set_cond_default_arcs_enabled 1
report_checks -path_delay max
sta::set_cond_default_arcs_enabled 0
report_checks -path_delay max
