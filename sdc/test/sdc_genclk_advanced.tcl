# Test advanced generated clock options and clock model queries for code coverage
# Targets: Clock.cc (generated clock model, edge-based, master clock),
#          WriteSdc.cc (writeGeneratedClock, writeClockPins),
#          Sdc.cc (clock creation and deletion paths)
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

############################################################
# Multiple clocks on same port and various generated clocks
############################################################

# Base clocks
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 -waveform {0 10} [get_ports clk2]
puts "PASS: base clocks"

# Virtual clock (no pin)
create_clock -name vclk -period 8
puts "PASS: virtual clock"

# Multiple clocks on same port (-add)
create_clock -name clk1_2x -period 5 -add [get_ports clk1]
puts "PASS: clock -add on same port"

# Asymmetric waveform clock
create_clock -name clk_asym -period 12 -waveform {0 3} -add [get_ports clk2]
puts "PASS: asymmetric waveform clock"

# Report clock properties
report_clock_properties
puts "PASS: report_clock_properties initial"

############################################################
# Generated clocks - divide_by
############################################################

create_generated_clock -name gclk_div2 -source [get_ports clk1] -divide_by 2 [get_pins reg1/Q]
puts "PASS: generated clock -divide_by 2"

create_generated_clock -name gclk_div3 -source [get_ports clk2] -divide_by 3 [get_pins reg3/Q]
puts "PASS: generated clock -divide_by 3"

############################################################
# Generated clocks - multiply_by
############################################################

create_generated_clock -name gclk_mul2 -source [get_ports clk1] -multiply_by 2 [get_pins reg2/Q]
puts "PASS: generated clock -multiply_by 2"

############################################################
# Generated clocks - edges
############################################################

# Edge-based generated clock
catch {
  create_generated_clock -name gclk_edge -source [get_ports clk1] -edges {1 3 5} [get_pins reg1/Q] -add
  puts "PASS: generated clock -edges"
}

############################################################
# Generated clock - edge shift
############################################################

catch {
  create_generated_clock -name gclk_shift -source [get_ports clk2] -edges {1 3 5} -edge_shift {0.0 0.5 1.0} [get_pins reg3/Q] -add
  puts "PASS: generated clock -edge_shift"
}

############################################################
# Report clock properties after generated clocks
############################################################

report_clock_properties
puts "PASS: report_clock_properties with generated"

############################################################
# Clock constraints on generated clocks
############################################################

# Source latency on generated clock
set_clock_latency -source 0.3 [get_clocks gclk_div2]
set_clock_latency -source -rise -max 0.4 [get_clocks gclk_div2]
set_clock_latency -source -fall -min 0.1 [get_clocks gclk_div2]
puts "PASS: source latency on generated clock"

# Network latency on generated clock
set_clock_latency 0.15 [get_clocks gclk_div3]
puts "PASS: network latency on generated clock"

# Clock uncertainty on generated clocks
set_clock_uncertainty -setup 0.15 [get_clocks gclk_div2]
set_clock_uncertainty -hold 0.08 [get_clocks gclk_div2]
puts "PASS: uncertainty on generated clock"

# Inter-clock uncertainty between generated and base
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks gclk_div2] -setup 0.2
set_clock_uncertainty -from [get_clocks gclk_div2] -to [get_clocks clk2] -hold 0.1
puts "PASS: inter-clock uncertainty with generated"

# Clock transition on generated clock
set_clock_transition 0.12 [get_clocks gclk_div2]
set_clock_transition -rise -max 0.15 [get_clocks gclk_mul2]
puts "PASS: transition on generated clock"

# Propagated clock on generated
set_propagated_clock [get_clocks gclk_div2]
puts "PASS: propagated on generated clock"

############################################################
# IO delays referencing generated clocks
############################################################

set_input_delay -clock gclk_div2 3.0 [get_ports in1]
set_input_delay -clock gclk_div2 -rise -max 3.5 [get_ports in2]
set_input_delay -clock gclk_div2 -fall -min 1.5 [get_ports in2]
puts "PASS: input delay with generated clock"

set_output_delay -clock gclk_mul2 2.0 [get_ports out1]
set_output_delay -clock gclk_div3 2.5 [get_ports out2]
puts "PASS: output delay with generated clock"

############################################################
# Clock groups involving generated clocks
############################################################

set_clock_groups -asynchronous -group {clk1 clk1_2x gclk_div2 gclk_mul2} -group {clk2 gclk_div3}
puts "PASS: clock groups with generated clocks"

############################################################
# Exception paths referencing generated clocks
############################################################

set_false_path -from [get_clocks gclk_div2] -to [get_clocks gclk_div3]
puts "PASS: false path between generated clocks"

set_multicycle_path -setup 3 -from [get_clocks clk1] -to [get_clocks gclk_div2]
puts "PASS: multicycle to generated clock"

############################################################
# Write SDC (exercises generated clock writing)
############################################################

set sdc_file1 [make_result_file sdc_genclk_native.sdc]
write_sdc -no_timestamp $sdc_file1
puts "PASS: write_sdc with generated clocks"

set sdc_file2 [make_result_file sdc_genclk_compat.sdc]
write_sdc -no_timestamp -compatible $sdc_file2
puts "PASS: write_sdc -compatible with generated clocks"

set sdc_file3 [make_result_file sdc_genclk_d6.sdc]
write_sdc -no_timestamp -digits 6 $sdc_file3
puts "PASS: write_sdc -digits 6 with generated clocks"

############################################################
# Report checks
############################################################

report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: report_checks with generated clocks"

report_checks -from [get_ports in2] -to [get_ports out2]
puts "PASS: report_checks path 2"

############################################################
# Delete and re-create clocks (exercises Clock.cc deletion)
############################################################

# Remove clock groups first
unset_clock_groups -asynchronous -all
puts "PASS: unset clock groups"

# Delete generated clocks
delete_generated_clock [get_clocks gclk_mul2]
puts "PASS: delete gclk_mul2"

# Unset latencies on gclk_div2 before delete
unset_clock_latency [get_clocks gclk_div2]
unset_clock_latency -source [get_clocks gclk_div2]
unset_propagated_clock [get_clocks gclk_div2]
puts "PASS: unset latencies on gclk_div2"

delete_generated_clock [get_clocks gclk_div2]
puts "PASS: delete gclk_div2"

delete_generated_clock [get_clocks gclk_div3]
puts "PASS: delete gclk_div3"

# Delete virtual clock
delete_clock [get_clocks vclk]
puts "PASS: delete virtual clock"

report_clock_properties
puts "PASS: report_clock_properties after deletions"

puts "ALL PASSED"
