# Test Sim.cc constant propagation, clock gating simulation,
# Levelize.cc deeper loop/level operations,
# and Sta.cc constraint-related functions.
# Targets: Sim.cc setPinValue, evalInstance, clockGateOutValue,
#          annotateGraphEdges, annotateVertexEdges, seedConstants,
#          propagateConstants, setConstraintConstPins, setConstFuncPins,
#          enqueueConstantPinInputs, removePropagatedValue,
#          Levelize.cc levelize, reportLoops, GraphLoop::report,
#          Sta.cc setLogicValue, findLogicConstants, clearLogicConstants,
#          setCaseAnalysis, removeCaseAnalysis, set/unset propagated clock,
#          setClockLatency, removeClockLatency, setClockInsertion,
#          setClockUncertainty, removeClockUncertainty,
#          setLatchBorrowLimit, setMinPulseWidth
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_data_check_gated.v
link_design search_data_check_gated

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports en]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_input_delay -clock clk 0.5 [get_ports rst]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]
set_output_delay -clock clk 2.0 [get_ports out3]

report_checks > /dev/null

############################################################
# set_logic_zero on multiple pins
############################################################
puts "--- set_logic_zero in1 ---"
set_logic_zero [get_ports in1]
set sv [sta::pin_sim_logic_value [get_pins and1/A1]]
puts "in1=0 and1/A1=$sv"
report_checks -path_delay max

puts "--- set_logic_zero in2 ---"
set_logic_zero [get_ports in2]
set sv2 [sta::pin_sim_logic_value [get_pins and1/A2]]
puts "in2=0 and1/A2=$sv2"
set sv_zn [sta::pin_sim_logic_value [get_pins and1/ZN]]
puts "and1/ZN=$sv_zn"
report_checks -path_delay max

############################################################
# set_logic_one
############################################################
puts "--- set_logic_one en ---"
set_logic_one [get_ports en]
set sv_en [sta::pin_sim_logic_value [get_pins clk_gate/A2]]
puts "en=1: clk_gate/A2=$sv_en"
set sv_gated [sta::pin_sim_logic_value [get_pins clk_gate/ZN]]
puts "gated_clk=$sv_gated"
report_checks -path_delay max

############################################################
# set_logic_one in1 (overwrite zero)
############################################################
puts "--- set_logic_one in1 (overwrite) ---"
set_logic_one [get_ports in1]
set sv_a1 [sta::pin_sim_logic_value [get_pins and1/A1]]
puts "in1=1 and1/A1=$sv_a1"
set sv_zn2 [sta::pin_sim_logic_value [get_pins and1/ZN]]
puts "and1/ZN=$sv_zn2 (in1=1,in2=0 -> 0)"
report_checks -path_delay max

############################################################
# Case analysis with rising/falling
############################################################
puts "--- case_analysis 0 on en ---"
set_case_analysis 0 [get_ports en]
set sv_gated_0 [sta::pin_sim_logic_value [get_pins clk_gate/ZN]]
puts "en=0: gated_clk=$sv_gated_0"
report_checks -path_delay max
unset_case_analysis [get_ports en]

puts "--- case_analysis 1 on en ---"
set_case_analysis 1 [get_ports en]
set sv_gated_1 [sta::pin_sim_logic_value [get_pins clk_gate/ZN]]
puts "en=1: gated_clk=$sv_gated_1"
report_checks -path_delay max
unset_case_analysis [get_ports en]

puts "--- case_analysis rising on rst ---"
set_case_analysis rising [get_ports rst]
report_checks -path_delay max
unset_case_analysis [get_ports rst]

puts "--- case_analysis falling on rst ---"
set_case_analysis falling [get_ports rst]
report_checks -path_delay max
unset_case_analysis [get_ports rst]

############################################################
# Constants are handled via case_analysis and logic_one/zero
# which drive Sim.cc propagation internally
############################################################
puts "--- Constant propagation via case_analysis ---"
set_case_analysis 0 [get_ports in1]
set_case_analysis 0 [get_ports in2]
set sv_zn3 [sta::pin_sim_logic_value [get_pins and1/ZN]]
puts "in1=0,in2=0: and1/ZN=$sv_zn3"
set sv_inv [sta::pin_sim_logic_value [get_pins inv1/ZN]]
puts "inv1/ZN=$sv_inv"
report_checks -path_delay max
unset_case_analysis [get_ports in1]
unset_case_analysis [get_ports in2]

############################################################
# Levelize operations
############################################################
puts "--- levelize ---"
sta::levelize

puts "--- report_loops ---"
sta::report_loops

############################################################
# Clock constraints
############################################################
puts "--- set_propagated_clock ---"
set_propagated_clock [get_clocks clk]
report_checks -path_delay max

puts "--- report_clock_skew after propagation ---"
report_clock_skew -setup
report_clock_skew -hold

puts "--- unset_propagated_clock ---"
unset_propagated_clock [get_clocks clk]

############################################################
# Clock latency
############################################################
puts "--- set_clock_latency -source ---"
set_clock_latency -source 0.2 [get_clocks clk]
report_checks -path_delay max

puts "--- set_clock_latency (network) ---"
set_clock_latency 0.1 [get_clocks clk]
report_checks -path_delay max

puts "--- unset_clock_latency ---"
unset_clock_latency [get_clocks clk]
unset_clock_latency -source [get_clocks clk]

############################################################
# Clock insertion delay
############################################################
puts "--- set_clock_latency -source -rise ---"
set_clock_latency -source -rise 0.15 [get_clocks clk]
report_checks -path_delay max

puts "--- set_clock_latency -source -fall ---"
set_clock_latency -source -fall 0.2 [get_clocks clk]
report_checks -path_delay max

puts "--- unset ---"
unset_clock_latency -source [get_clocks clk]

############################################################
# Clock uncertainty
############################################################
puts "--- set_clock_uncertainty ---"
set_clock_uncertainty 0.5 [get_clocks clk]
report_checks -path_delay max

puts "--- set_clock_uncertainty -setup ---"
set_clock_uncertainty -setup 0.3 [get_clocks clk]
report_checks -path_delay max

puts "--- set_clock_uncertainty -hold ---"
set_clock_uncertainty -hold 0.2 [get_clocks clk]
report_checks -path_delay min

puts "--- unset_clock_uncertainty ---"
unset_clock_uncertainty [get_clocks clk]

############################################################
# Latch borrow limit
############################################################
puts "--- set_max_time_borrow ---"
set_max_time_borrow 1.0 [get_clocks clk]
report_checks -path_delay max

############################################################
# Min pulse width
############################################################
report_pulse_width_checks

############################################################
# report_constant
############################################################
puts "--- report_constant ---"
set_case_analysis 0 [get_ports in1]
report_constant [get_ports in1]
report_constant [get_cells and1]
unset_case_analysis [get_ports in1]

############################################################
# Disable timing on various targets
############################################################
puts "--- set_disable_timing port ---"
set_disable_timing [get_ports in1]
report_checks -path_delay max
unset_disable_timing [get_ports in1]

puts "--- set_disable_timing instance ---"
set_disable_timing [get_cells buf1]
report_checks -path_delay max
unset_disable_timing [get_cells buf1]
report_checks -path_delay max

############################################################
# CRPR settings
############################################################
puts "--- CRPR settings ---"
sta::set_crpr_enabled 1
puts "crpr_enabled: [sta::crpr_enabled]"
sta::set_crpr_mode "same_pin"
puts "crpr_mode: [sta::crpr_mode]"
report_checks -path_delay max
sta::set_crpr_mode "same_transition"
puts "crpr_mode: [sta::crpr_mode]"
report_checks -path_delay max
sta::set_crpr_enabled 0

############################################################
# Recovery/removal checks
############################################################
puts "--- recovery/removal checks ---"
sta::set_recovery_removal_checks_enabled 1
report_checks -path_delay max
sta::set_recovery_removal_checks_enabled 0

############################################################
# Gated clock checks
############################################################
puts "--- gated clock checks ---"
sta::set_gated_clk_checks_enabled 1
sta::set_propagate_gated_clock_enable 1
report_checks -path_delay max
sta::set_gated_clk_checks_enabled 0
sta::set_propagate_gated_clock_enable 0

############################################################
# Timing derate
############################################################
puts "--- timing_derate ---"
set_timing_derate -early 0.95
set_timing_derate -late 1.05
report_checks -path_delay max
report_checks -path_delay min
unset_timing_derate

############################################################
# Tag/group reporting (for Tag.cc coverage)
############################################################
puts "--- tag/group reporting ---"
puts "tag_count: [sta::tag_count]"
puts "tag_group_count: [sta::tag_group_count]"
puts "clk_info_count: [sta::clk_info_count]"
puts "path_count: [sta::path_count]"

puts "--- report internal ---"
sta::report_tags
sta::report_clk_infos
sta::report_tag_groups
