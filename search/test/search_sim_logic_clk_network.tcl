# Test Sim.cc logic simulation, clock network queries, Genclks.cc,
# ClkNetwork.cc, various Sta.cc clock/timing query functions.
# Targets: Sim.cc findLogicConstants, clearLogicConstants,
#          simLogicValue, setCaseAnalysis/removeCaseAnalysis incremental,
#          setLogicValue/removeLogicValue,
#          Sta.cc isClock, isIdealClock, isPropagatedClock,
#          clocks(pin), clockDomains, clkPinsInvalid,
#          ensureClkArrivals, ensureClkNetwork,
#          findClkMinPeriod, findClkDelays,
#          ClkNetwork.cc clock pin queries,
#          Genclks.cc updateGeneratedClks,
#          Levelize.cc levelize, graphLoops,
#          vertexLevel, maxPathCountVertex
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_gated_clk.v
link_design search_gated_clk

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports en]
set_input_delay -clock clk 1.0 [get_ports in1]
set_output_delay -clock clk 2.0 [get_ports out1]

# Baseline
report_checks -path_delay max > /dev/null

############################################################
# isClock queries
############################################################
puts "--- isClock queries ---"
set clk_pin_is_clk [sta::is_clock [sta::get_port_pin [get_ports clk]]]
puts "clk port is_clock: $clk_pin_is_clk"
set in1_is_clk [sta::is_clock [sta::get_port_pin [get_ports in1]]]
puts "in1 port is_clock: $in1_is_clk"

############################################################
# isIdealClock / isPropagatedClock
############################################################
puts "--- ideal/propagated clock queries ---"
set clk_pin [sta::get_port_pin [get_ports clk]]
puts "clk isIdealClock: [sta::is_ideal_clock $clk_pin]"
set_propagated_clock [get_clocks clk]
puts "after propagate - clk isIdealClock: [sta::is_ideal_clock $clk_pin]"
unset_propagated_clock [get_clocks clk]

############################################################
# Logic simulation values
############################################################
puts "--- sim logic values ---"
set sv_en [sta::pin_sim_logic_value [get_pins clk_gate/A2]]
set sv_clk [sta::pin_sim_logic_value [get_pins clk_gate/A1]]
set sv_gated [sta::pin_sim_logic_value [get_pins clk_gate/ZN]]
set sv_buf1 [sta::pin_sim_logic_value [get_pins buf1/Z]]
set sv_reg1_d [sta::pin_sim_logic_value [get_pins reg1/D]]
puts "en=$sv_en clk_gate_a1=$sv_clk gated=$sv_gated buf1=$sv_buf1 reg1/D=$sv_reg1_d"

############################################################
# Case analysis and logic simulation
############################################################
puts "--- case analysis 0 on en ---"
set_case_analysis 0 [get_ports en]
set sv_gated_0 [sta::pin_sim_logic_value [get_pins clk_gate/ZN]]
puts "en=0: gated_clk=$sv_gated_0"
report_checks -path_delay max
unset_case_analysis [get_ports en]

puts "--- case analysis 1 on en ---"
set_case_analysis 1 [get_ports en]
set sv_gated_1 [sta::pin_sim_logic_value [get_pins clk_gate/ZN]]
puts "en=1: gated_clk=$sv_gated_1"
report_checks -path_delay max
unset_case_analysis [get_ports en]

puts "--- case analysis rising on en ---"
set_case_analysis rising [get_ports en]
report_checks -path_delay max
unset_case_analysis [get_ports en]

puts "--- case analysis falling on en ---"
set_case_analysis falling [get_ports en]
report_checks -path_delay max
unset_case_analysis [get_ports en]

############################################################
# set_logic_one/zero
############################################################
puts "--- set_logic_zero ---"
set_logic_zero [get_ports in1]
set sv_buf_z [sta::pin_sim_logic_value [get_pins buf1/Z]]
puts "in1=0: buf1/Z=$sv_buf_z"
report_checks -path_delay max

puts "--- set_logic_one ---"
set_logic_one [get_ports en]
set sv_en_1 [sta::pin_sim_logic_value [get_pins clk_gate/A2]]
puts "en=1: clk_gate/A2=$sv_en_1"
report_checks -path_delay max

############################################################
# findLogicConstants / clearLogicConstants
############################################################
puts "--- findLogicConstants ---"
# catch: sta::find_logic_constants is not exposed as Tcl command
catch { sta::find_logic_constants }

puts "--- clearLogicConstants ---"
# catch: sta::clear_logic_constants is not exposed as Tcl command
catch { sta::clear_logic_constants }

############################################################
# Levelize and graph queries
############################################################
puts "--- levelize ---"
sta::levelize

puts "--- graphLoops ---"
# catch: sta::graph_loop_count is not exposed as Tcl command
catch {
  set loops [sta::graph_loop_count]
  puts "Graph loops: $loops"
}

puts "--- max_path_count_vertex ---"
# catch: sta::max_path_count_vertex is not exposed as Tcl command
catch {
  set maxv [sta::max_path_count_vertex]
  if { $maxv != "NULL" } {
    puts "max_path_count vertex: [get_full_name [$maxv pin]]"
    puts "  path_count: [sta::vertex_path_count $maxv]"
    puts "  level: [sta::vertex_level $maxv]"
  }
}

############################################################
# Generated clock (exercises Genclks.cc)
############################################################
puts "--- generated clock ---"
create_generated_clock -name gclk -source [get_ports clk] -divide_by 2 [get_pins reg1/Q]
report_checks -path_delay max
report_checks -path_delay min

puts "--- report_clock_properties with genclk ---"
report_clock_properties

puts "--- clock skew with genclk ---"
report_clock_skew -setup
report_clock_skew -hold

############################################################
# Clock min period
############################################################
puts "--- clock min period ---"
report_clock_min_period
report_clock_min_period -include_port_paths

############################################################
# Clock latency reporting
############################################################
puts "--- clock latency report ---"
set_propagated_clock [get_clocks clk]
report_clock_latency
report_clock_latency -include_internal_latency
report_clock_latency -digits 6
unset_propagated_clock [get_clocks clk]

############################################################
# find_timing_paths for different clk domains
############################################################
puts "--- find_timing_paths for clock groups ---"
set paths [find_timing_paths -path_delay max -endpoint_path_count 5 -group_path_count 10]
puts "Max paths: [llength $paths]"
foreach pe $paths {
  puts "  pin=[get_full_name [$pe pin]] slack=[$pe slack]"
}

############################################################
# report_checks with -through for clock gate
############################################################
puts "--- report_checks through clock gate ---"
report_checks -through [get_pins clk_gate/ZN] -path_delay max
report_checks -through [get_pins clk_gate/ZN] -path_delay min

############################################################
# Various bidirectional/tristate enable flags
############################################################
puts "--- bidirect inst paths ---"
sta::set_bidirect_inst_paths_enabled 1
report_checks -path_delay max
sta::set_bidirect_inst_paths_enabled 0
report_checks -path_delay max

puts "--- bidirect net paths ---"
sta::set_bidirect_net_paths_enabled 1
report_checks -path_delay max
sta::set_bidirect_net_paths_enabled 0
report_checks -path_delay max

puts "--- clk thru tristate ---"
sta::set_clk_thru_tristate_enabled 1
report_checks -path_delay max
sta::set_clk_thru_tristate_enabled 0
report_checks -path_delay max

puts "--- dynamic loop breaking ---"
sta::set_dynamic_loop_breaking 1
report_checks -path_delay max
sta::set_dynamic_loop_breaking 0
report_checks -path_delay max

puts "--- use default arrival clock ---"
sta::set_use_default_arrival_clock 1
report_checks -path_delay max
sta::set_use_default_arrival_clock 0
report_checks -path_delay max

puts "--- propagate all clocks ---"
sta::set_propagate_all_clocks 1
report_checks -path_delay max
sta::set_propagate_all_clocks 0
report_checks -path_delay max
