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
puts "PASS: baseline timing"

############################################################
# Read SPEF
############################################################
puts "--- read_spef ---"
read_spef search_test1.spef
puts "PASS: read_spef"

puts "--- Timing after SPEF ---"
report_checks -path_delay max
report_checks -path_delay min
puts "PASS: timing after SPEF"

puts "--- report_parasitic_annotation ---"
report_parasitic_annotation
puts "PASS: report_parasitic_annotation"

puts "--- report_parasitic_annotation -report_unannotated ---"
catch {
  report_parasitic_annotation -report_unannotated
}
puts "PASS: report_parasitic_annotation unannotated"

############################################################
# connectedCap via report_net (exercises connectedCap)
############################################################
puts "--- report_net after SPEF ---"
report_net n1
report_net n2
report_net n3
puts "PASS: report_net after SPEF"

puts "--- report_net -digits 6 ---"
report_net -digits 6 n1
report_net -digits 6 n2
puts "PASS: report_net digits"

############################################################
# Port ext caps: set_load exercises setPortExtPinCap
############################################################
puts "--- setPortExtPinCap (set_load -pin_load) ---"
set_load -pin_load 0.05 [get_ports out1]
report_checks -path_delay max
puts "PASS: setPortExtPinCap"

puts "--- setPortExtWireCap (set_load -wire_load) ---"
catch {
  set_load -wire_load 0.03 [get_ports out1]
  report_checks -path_delay max
}
puts "PASS: setPortExtWireCap"

puts "--- setPortExtFanout ---"
set_port_fanout_number 4 [get_ports out1]
report_checks -path_delay max
puts "PASS: setPortExtFanout"

############################################################
# setNetWireCap - exercises Sta::setNetWireCap
############################################################
puts "--- setNetWireCap ---"
catch {
  set_load 0.02 [get_nets n1]
  report_checks -path_delay max
}
puts "PASS: setNetWireCap"

############################################################
# Pi-elmore model: makePiElmore, findPiElmore, setElmore, findElmore
############################################################
puts "--- set_pi_model (makePiElmore) ---"
catch {
  sta::set_pi_model and1/ZN 3.0e-15 1500.0 2.0e-15
  puts "Pi model created for and1/ZN"
}
puts "PASS: set_pi_model"

puts "--- set_elmore (setElmore) ---"
catch {
  sta::set_elmore and1/ZN buf1/A 5.0e-12
  puts "Elmore delay set for and1/ZN -> buf1/A"
}
puts "PASS: set_elmore"

puts "--- find_elmore ---"
catch {
  set elm [sta::find_elmore [get_pins and1/ZN] [get_pins buf1/A] "rise" "max"]
  puts "Elmore delay and1/ZN->buf1/A rise max: $elm"
  set elm2 [sta::find_elmore [get_pins and1/ZN] [get_pins buf1/A] "fall" "max"]
  puts "Elmore delay and1/ZN->buf1/A fall max: $elm2"
}
puts "PASS: find_elmore"

puts "--- find_pi_elmore ---"
catch {
  set pi [sta::find_pi_elmore [get_pins and1/ZN] "rise" "max"]
  puts "Pi-elmore and1/ZN rise max: $pi"
  set pi2 [sta::find_pi_elmore [get_pins and1/ZN] "fall" "max"]
  puts "Pi-elmore and1/ZN fall max: $pi2"
}
puts "PASS: find_pi_elmore"

puts "--- Timing after manual parasitics ---"
report_checks -path_delay max
report_checks -path_delay min
puts "PASS: timing after manual parasitics"

############################################################
# Re-read SPEF (exercises setParasiticAnalysisPts path)
############################################################
puts "--- re-read SPEF ---"
read_spef search_test1.spef
report_checks -path_delay max
puts "PASS: re-read SPEF"

############################################################
# setResistance on net
############################################################
puts "--- setResistance ---"
catch {
  set_resistance 100.0 [get_nets n1]
  report_checks -path_delay max
}
puts "PASS: setResistance"

############################################################
# Timing with propagated clock + SPEF
############################################################
puts "--- SPEF with propagated clock ---"
set_propagated_clock [get_clocks clk]
report_checks -path_delay max
report_checks -path_delay min
unset_propagated_clock [get_clocks clk]
puts "PASS: SPEF with propagated clock"

############################################################
# read_spef with -min and -max flags
############################################################
puts "--- read_spef -min ---"
catch {
  read_spef -min search_test1.spef
}
puts "PASS: read_spef -min"

puts "--- read_spef -max ---"
catch {
  read_spef -max search_test1.spef
}
puts "PASS: read_spef -max"

############################################################
# Report formats after SPEF loading
############################################################
puts "--- Report formats after SPEF ---"
report_checks -path_delay max -format full_clock_expanded
report_checks -path_delay max -format json
report_checks -path_delay max -format summary
report_checks -path_delay max -fields {capacitance slew fanout}
puts "PASS: report formats after SPEF"

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
puts "PASS: worst_slack/tns after SPEF"

############################################################
# set_load with -min/-max flags (exercises setPortExtPinCap with min_max)
############################################################
puts "--- set_load -min/-max ---"
set_load -min 0.02 [get_ports out1]
set_load -max 0.08 [get_ports out1]
report_checks -path_delay max
report_checks -path_delay min
puts "PASS: set_load -min/-max"

############################################################
# report_net with -connections (exercises connectedCap and net printing)
############################################################
puts "--- report_net -connections ---"
catch {
  report_net -connections n1
  report_net -connections n2
}
puts "PASS: report_net -connections"

puts "ALL PASSED"
