# Test clock operations: removeClock, clock properties,
# generated clock edge/invert, clock uncertainty inter-clock,
# clock insertion, clock latency source variants,
# findClock, clock waveform edge handling.
# Targets: Sdc.cc removeClock, makeClockAfter, findClock,
#          clkFindLeafPins, makeGeneratedClock edge/multiply/divide/invert,
#          setClockInsertion, removeClockInsertion,
#          setClockLatency various source/nonsource/early/late,
#          removeClockLatency,
#          Clock.cc generated clock operations, waveform edge handling,
#          addToPins, clkEdgeTimes, clkEdge,
#          WriteSdc.cc writeClocks, writeGeneratedClock, writeClock,
#          writeClockSlews, writeClockUncertainty, writeClockLatencies,
#          writeClockInsertions, writeInterClockUncertainties,
#          writePropagatedClkPins
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

set_input_delay -clock [create_clock -name clk1 -period 10 [get_ports clk1]] 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_output_delay -clock clk1 3.0 [get_ports out1]

############################################################
# Create clocks with different waveforms
############################################################
puts "--- clock with custom waveform ---"
create_clock -name clk2 -period 20 -waveform {5 15} [get_ports clk2]
set_input_delay -clock clk2 2.0 [get_ports in3]
set_output_delay -clock clk2 3.0 [get_ports out2]
report_checks

puts "--- clock with asymmetric waveform ---"
create_clock -name vclk1 -period 8 -waveform {0 3}

puts "--- clock with -add ---"
create_clock -name clk1_alt -period 5 -add [get_ports clk1]
report_checks

############################################################
# Generated clocks with various options
############################################################
puts "--- generated clock divide_by ---"
create_generated_clock -name gclk_div2 -source [get_ports clk1] -divide_by 2 [get_pins reg1/Q]
report_checks

puts "--- generated clock multiply_by ---"
create_generated_clock -name gclk_mul3 -source [get_ports clk2] -multiply_by 3 [get_pins reg3/Q]
report_checks

puts "--- generated clock edges ---"
create_generated_clock -name gclk_edge -source [get_ports clk1] -edges {1 3 5} [get_pins reg2/Q]
report_checks

puts "--- generated clock invert ---"
catch {
  create_generated_clock -name gclk_inv -source [get_ports clk1] -divide_by 1 -invert [get_pins reg1/Q] -add
  report_checks
}

############################################################
# Propagated clock
############################################################
puts "--- set_propagated_clock ---"
set_propagated_clock [get_clocks clk1]
set_propagated_clock [get_clocks clk2]
report_checks

puts "--- set_propagated_clock on pin ---"
catch {
  set_propagated_clock [get_ports clk1]
}

############################################################
# Clock slew/transition
############################################################
puts "--- clock transition ---"
set_clock_transition -rise -max 0.15 [get_clocks clk1]
set_clock_transition -fall -min 0.08 [get_clocks clk1]
set_clock_transition 0.1 [get_clocks clk2]
set_clock_transition -rise 0.12 [get_clocks clk1]
set_clock_transition -fall 0.09 [get_clocks clk1]
report_checks

############################################################
# Clock latency - source and non-source
############################################################
puts "--- clock latency source ---"
set_clock_latency -source 0.5 [get_clocks clk1]
set_clock_latency -source -early 0.3 [get_clocks clk1]
set_clock_latency -source -late 0.6 [get_clocks clk1]
set_clock_latency -source -rise -max 0.65 [get_clocks clk1]
set_clock_latency -source -fall -min 0.25 [get_clocks clk1]
report_checks

puts "--- clock latency non-source ---"
set_clock_latency 0.2 [get_clocks clk2]
set_clock_latency -rise -max 0.4 [get_clocks clk2]
set_clock_latency -fall -min 0.1 [get_clocks clk2]
report_checks

############################################################
# Clock insertion
############################################################
puts "--- clock insertion ---"
catch {
  set_clock_latency -source -rise -early 0.1 [get_clocks clk1]
  set_clock_latency -source -rise -late 0.3 [get_clocks clk1]
  set_clock_latency -source -fall -early 0.15 [get_clocks clk1]
  set_clock_latency -source -fall -late 0.35 [get_clocks clk1]
}
report_checks

############################################################
# Clock uncertainty - simple
############################################################
puts "--- clock uncertainty ---"
set_clock_uncertainty -setup 0.2 [get_clocks clk1]
set_clock_uncertainty -hold 0.1 [get_clocks clk1]
set_clock_uncertainty 0.15 [get_clocks clk2]
report_checks

############################################################
# Inter-clock uncertainty
############################################################
puts "--- inter-clock uncertainty ---"
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup 0.3
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -hold 0.15
set_clock_uncertainty -from [get_clocks clk2] -to [get_clocks clk1] -setup 0.28
set_clock_uncertainty -from [get_clocks clk2] -to [get_clocks clk1] -hold 0.12
report_checks

############################################################
# Clock uncertainty on pin
############################################################
puts "--- clock uncertainty on pin ---"
catch {
  set_clock_uncertainty -setup 0.25 [get_ports clk1]
  set_clock_uncertainty -hold 0.08 [get_ports clk1]
}
report_checks

############################################################
# Write SDC
############################################################
puts "--- write_sdc ---"
set sdc1 [make_result_file sdc_clock_ops1.sdc]
write_sdc -no_timestamp $sdc1

puts "--- write_sdc compatible ---"
set sdc2 [make_result_file sdc_clock_ops2.sdc]
write_sdc -no_timestamp -compatible $sdc2

############################################################
# Remove clock and re-create
############################################################
puts "--- remove_clock ---"
catch {
  remove_clock vclk1
  report_checks
}

############################################################
# Clock properties reporting
############################################################
puts "--- report_clock_properties ---"
report_clock_properties
report_clock_properties clk1
report_clock_properties clk2

############################################################
# Read SDC back
############################################################
puts "--- read_sdc ---"
read_sdc $sdc1
report_checks

############################################################
# Remove clock latency
############################################################
puts "--- unset_clock_latency ---"
catch {
  unset_clock_latency -source [get_clocks clk1]
  report_checks
}

############################################################
# Remove clock uncertainty
############################################################
puts "--- unset_clock_uncertainty ---"
catch {
  unset_clock_uncertainty -setup [get_clocks clk1]
  unset_clock_uncertainty -hold [get_clocks clk1]
  report_checks
}

############################################################
# Remove inter-clock uncertainty
############################################################
puts "--- unset inter-clock uncertainty ---"
catch {
  unset_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup
  unset_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -hold
  report_checks
}

############################################################
# Remove propagated clock
############################################################
puts "--- unset_propagated_clock ---"
catch {
  unset_propagated_clock [get_clocks clk1]
  unset_propagated_clock [get_clocks clk2]
  report_checks
}

############################################################
# Final write
############################################################
set sdc3 [make_result_file sdc_clock_ops3.sdc]
write_sdc -no_timestamp $sdc3
