# Test SPEF reading, parasitic operations, pi-elmore models,
# connectedCap, reportParasiticAnnotation.
# Targets: Sta.cc readSpef, setParasiticAnalysisPts,
#          makeParasiticAnalysisPts, findPiElmore, makePiElmore,
#          findElmore, setElmore, connectedCap (Pin and Net variants),
#          reportParasiticAnnotation,
#          setNetWireCap, setResistance, setPortExtPinCap, portExtCaps,
#          Search.cc arrivalInvalid, requiredInvalid via delay invalidation
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

# Baseline timing (no parasitics)
report_checks -path_delay max > /dev/null
puts "--- Baseline timing (no parasitics) ---"
report_checks -path_delay max
report_checks -path_delay min

############################################################
# Read SPEF
############################################################
puts "--- read_spef ---"
read_spef search_test1.spef

puts "--- Timing after SPEF ---"
report_checks -path_delay max
report_checks -path_delay min

puts "--- report_parasitic_annotation ---"
report_parasitic_annotation

puts "--- report_parasitic_annotation -report_unannotated ---"
report_parasitic_annotation -report_unannotated

############################################################
# connectedCap via report_net (exercises connectedCap)
############################################################
puts "--- report_net after SPEF ---"
report_net n1
report_net n2
report_net n3

puts "--- report_net -digits 6 ---"
report_net -digits 6 n1
report_net -digits 6 n2

############################################################
# Port ext caps: set_load exercises setPortExtPinCap
############################################################
puts "--- setPortExtPinCap (set_load -pin_load) ---"
set_load -pin_load 0.05 [get_ports out1]
report_checks -path_delay max

puts "--- setPortExtWireCap (set_load -wire_load) ---"
set_load -wire_load 0.03 [get_ports out1]
report_checks -path_delay max

puts "--- setPortExtFanout ---"
set_port_fanout_number 4 [get_ports out1]
report_checks -path_delay max

############################################################
# setNetWireCap - exercises Sta::setNetWireCap
############################################################
puts "--- setNetWireCap ---"
set_load 0.02 [get_nets n1]
report_checks -path_delay max

############################################################
# Pi-elmore model: makePiElmore, findPiElmore, setElmore, findElmore
############################################################
puts "--- set_pi_model (makePiElmore) ---"
sta::set_pi_model and1/ZN 3.0e-15 1500.0 2.0e-15
puts "Pi model created for and1/ZN"

puts "--- set_elmore (setElmore) ---"
sta::set_elmore and1/ZN buf1/A 5.0e-12
puts "Elmore delay set for and1/ZN -> buf1/A"

puts "--- find_elmore ---"
set elm [sta::find_elmore [get_pins and1/ZN] [get_pins buf1/A] "rise" "max"]
puts "Elmore delay and1/ZN->buf1/A rise max: $elm"
set elm2 [sta::find_elmore [get_pins and1/ZN] [get_pins buf1/A] "fall" "max"]
puts "Elmore delay and1/ZN->buf1/A fall max: $elm2"

puts "--- find_pi_elmore ---"
set pi [sta::find_pi_elmore [get_pins and1/ZN] "rise" "max"]
puts "Pi-elmore and1/ZN rise max: $pi"
set pi2 [sta::find_pi_elmore [get_pins and1/ZN] "fall" "max"]
puts "Pi-elmore and1/ZN fall max: $pi2"

puts "--- Timing after manual parasitics ---"
report_checks -path_delay max
report_checks -path_delay min

############################################################
# Re-read SPEF (exercises setParasiticAnalysisPts path)
############################################################
puts "--- re-read SPEF ---"
read_spef search_test1.spef
report_checks -path_delay max

############################################################
# setResistance on net
############################################################
puts "--- setResistance ---"
set_resistance 100.0 [get_nets n1]
report_checks -path_delay max

############################################################
# Timing with propagated clock + SPEF
############################################################
puts "--- SPEF with propagated clock ---"
set_propagated_clock [get_clocks clk]
report_checks -path_delay max
report_checks -path_delay min
unset_propagated_clock [get_clocks clk]

############################################################
# read_spef with -min and -max flags
############################################################
puts "--- read_spef -min ---"
read_spef -min search_test1.spef

puts "--- read_spef -max ---"
read_spef -max search_test1.spef

############################################################
# Report formats after SPEF loading
############################################################
puts "--- Report formats after SPEF ---"
report_checks -path_delay max -format full_clock_expanded
report_checks -path_delay max -format json
report_checks -path_delay max -format summary
report_checks -path_delay max -fields {capacitance slew fanout}

############################################################
# worst_slack and tns with parasitics
############################################################
puts "--- worst_slack and tns after SPEF ---"
set ws_max [worst_slack -max]
puts "worst_slack max: $ws_max"
set ws_min [worst_slack -min]
puts "worst_slack min: $ws_min"
set tns_max [total_negative_slack -max]
puts "tns max: $tns_max"
set tns_min [total_negative_slack -min]
puts "tns min: $tns_min"

############################################################
# set_load with -min/-max flags (exercises setPortExtPinCap with min_max)
############################################################
puts "--- set_load -min/-max ---"
set_load -min 0.02 [get_ports out1]
set_load -max 0.08 [get_ports out1]
report_checks -path_delay max
report_checks -path_delay min

############################################################
# report_net with -connections (exercises connectedCap and net printing)
############################################################
puts "--- report_net -connections ---"
report_net -connections n1
report_net -connections n2
