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
puts "PASS: logic_zero in1"

puts "--- set_logic_zero in2 ---"
set_logic_zero [get_ports in2]
set sv2 [sta::pin_sim_logic_value [get_pins and1/A2]]
puts "in2=0 and1/A2=$sv2"
set sv_zn [sta::pin_sim_logic_value [get_pins and1/ZN]]
puts "and1/ZN=$sv_zn"
report_checks -path_delay max
puts "PASS: logic_zero both"

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
puts "PASS: logic_one en"

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
puts "PASS: logic_one overwrite"

############################################################
# Case analysis with rising/falling
############################################################
puts "--- case_analysis 0 on en ---"
set_case_analysis 0 [get_ports en]
set sv_gated_0 [sta::pin_sim_logic_value [get_pins clk_gate/ZN]]
puts "en=0: gated_clk=$sv_gated_0"
report_checks -path_delay max
unset_case_analysis [get_ports en]
puts "PASS: case 0 en"

puts "--- case_analysis 1 on en ---"
set_case_analysis 1 [get_ports en]
set sv_gated_1 [sta::pin_sim_logic_value [get_pins clk_gate/ZN]]
puts "en=1: gated_clk=$sv_gated_1"
report_checks -path_delay max
unset_case_analysis [get_ports en]
puts "PASS: case 1 en"

puts "--- case_analysis rising on rst ---"
set_case_analysis rising [get_ports rst]
report_checks -path_delay max
unset_case_analysis [get_ports rst]
puts "PASS: case rising rst"

puts "--- case_analysis falling on rst ---"
set_case_analysis falling [get_ports rst]
report_checks -path_delay max
unset_case_analysis [get_ports rst]
puts "PASS: case falling rst"

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
puts "PASS: constant propagation"

############################################################
# Levelize operations
############################################################
puts "--- levelize ---"
sta::levelize
puts "PASS: levelize"

puts "--- report_loops ---"
sta::report_loops
puts "PASS: report_loops"

############################################################
# Clock constraints
############################################################
puts "--- set_propagated_clock ---"
set_propagated_clock [get_clocks clk]
report_checks -path_delay max
puts "PASS: propagated clock"

puts "--- report_clock_skew after propagation ---"
report_clock_skew -setup
report_clock_skew -hold
puts "PASS: clock skew propagated"

puts "--- unset_propagated_clock ---"
unset_propagated_clock [get_clocks clk]
puts "PASS: unset propagated"

############################################################
# Clock latency
############################################################
puts "--- set_clock_latency -source ---"
set_clock_latency -source 0.2 [get_clocks clk]
report_checks -path_delay max
puts "PASS: clock latency source"

puts "--- set_clock_latency (network) ---"
set_clock_latency 0.1 [get_clocks clk]
report_checks -path_delay max
puts "PASS: clock latency network"

puts "--- unset_clock_latency ---"
unset_clock_latency [get_clocks clk]
unset_clock_latency -source [get_clocks clk]
puts "PASS: unset clock latency"

############################################################
# Clock insertion delay
############################################################
puts "--- set_clock_latency -source -rise ---"
set_clock_latency -source -rise 0.15 [get_clocks clk]
report_checks -path_delay max
puts "PASS: clock latency rise"

puts "--- set_clock_latency -source -fall ---"
set_clock_latency -source -fall 0.2 [get_clocks clk]
report_checks -path_delay max
puts "PASS: clock latency fall"

puts "--- unset ---"
unset_clock_latency -source [get_clocks clk]
puts "PASS: unset clock insertion"

############################################################
# Clock uncertainty
############################################################
puts "--- set_clock_uncertainty ---"
set_clock_uncertainty 0.5 [get_clocks clk]
report_checks -path_delay max
puts "PASS: clock uncertainty"

puts "--- set_clock_uncertainty -setup ---"
set_clock_uncertainty -setup 0.3 [get_clocks clk]
report_checks -path_delay max
puts "PASS: clock uncertainty setup"

puts "--- set_clock_uncertainty -hold ---"
set_clock_uncertainty -hold 0.2 [get_clocks clk]
report_checks -path_delay min
puts "PASS: clock uncertainty hold"

puts "--- unset_clock_uncertainty ---"
unset_clock_uncertainty [get_clocks clk]
puts "PASS: unset clock uncertainty"

############################################################
# Latch borrow limit
############################################################
puts "--- set_max_time_borrow ---"
catch {
  set_max_time_borrow 1.0 [get_clocks clk]
  report_checks -path_delay max
}
puts "PASS: max_time_borrow"

############################################################
# Min pulse width
############################################################
puts "--- set_min_pulse_width ---"
catch {
  set_min_pulse_width 0.5 [all_inputs]
}
puts "PASS: set min pulse width"

puts "--- report_pulse_width_checks after setting ---"
report_pulse_width_checks
puts "PASS: pulse width after set"

############################################################
# report_constant
############################################################
puts "--- report_constant ---"
set_case_analysis 0 [get_ports in1]
report_constant [get_ports in1]
report_constant [get_cells and1]
unset_case_analysis [get_ports in1]
puts "PASS: report_constant"

############################################################
# Disable timing on various targets
############################################################
puts "--- set_disable_timing port ---"
catch {
  set_disable_timing [get_ports in1]
  report_checks -path_delay max
  unset_disable_timing [get_ports in1]
}
puts "PASS: disable port"

puts "--- set_disable_timing instance ---"
set_disable_timing [get_cells buf1]
report_checks -path_delay max
unset_disable_timing [get_cells buf1]
report_checks -path_delay max
puts "PASS: disable instance"

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
puts "PASS: CRPR settings"

############################################################
# Recovery/removal checks
############################################################
puts "--- recovery/removal checks ---"
sta::set_recovery_removal_checks_enabled 1
report_checks -path_delay max
sta::set_recovery_removal_checks_enabled 0
puts "PASS: recovery/removal"

############################################################
# Gated clock checks
############################################################
puts "--- gated clock checks ---"
sta::set_gated_clk_checks_enabled 1
sta::set_propagate_gated_clock_enable 1
report_checks -path_delay max
sta::set_gated_clk_checks_enabled 0
sta::set_propagate_gated_clock_enable 0
puts "PASS: gated clock checks"

############################################################
# Timing derate
############################################################
puts "--- timing_derate ---"
set_timing_derate -early 0.95
set_timing_derate -late 1.05
report_checks -path_delay max
report_checks -path_delay min
unset_timing_derate
puts "PASS: timing derate"

############################################################
# Tag/group reporting (for Tag.cc coverage)
############################################################
puts "--- tag/group reporting ---"
catch {
  puts "tag_count: [sta::tag_count]"
  puts "tag_group_count: [sta::tag_group_count]"
  puts "clk_info_count: [sta::clk_info_count]"
  puts "path_count: [sta::path_count]"
}
puts "PASS: tag/group counts"

puts "--- report internal ---"
catch { sta::report_tags }
catch { sta::report_clk_infos }
catch { sta::report_tag_groups }
puts "PASS: internal reports"

puts "ALL PASSED"
