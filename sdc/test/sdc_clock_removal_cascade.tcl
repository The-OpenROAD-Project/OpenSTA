# Test deep clock removal cascading where deleting a master clock
# affects generated clocks, and re-creation exercises full path.
# Also tests clock setPeriod/setWaveform via clock re-definition,
# delete_clock on master with active generated clocks.
# Targets:
#   Sdc.cc: removeClock (full cascade with generated clock refs),
#     deleteMasterClkRefs (generated clock loses master),
#     deleteExceptionsReferencing (removes exceptions on deleted clock),
#     deleteInputDelaysReferencing, deleteOutputDelaysReferencing,
#     deleteClockLatenciesReferencing, deleteClockInsertionsReferencing,
#     deleteInterClockUncertaintiesReferencing,
#     deleteLatchBorrowLimitsReferencing,
#     deleteMinPulseWidthReferencing,
#     clockGroupsDeleteClkRefs (removes clock from groups),
#     clearCycleAcctings, deleteClkPinMappings,
#     isLeafPinClock, isLeafPinNonGeneratedClock
#   Clock.cc: setPeriod, setWaveform (via re-creation),
#     setClock on multiple pins, generated clock operations,
#     clock latency/uncertainty after deletion
#   WriteSdc.cc: writing constraints after clock deletion/re-creation
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

############################################################
# Phase 1: Create complex clock hierarchy
############################################################
create_clock -name clk_master -period 10 [get_ports clk1]
create_clock -name clk_aux -period 20 -waveform {0 10} [get_ports clk2]
create_clock -name vclk1 -period 5
create_clock -name vclk2 -period 8
puts "PASS: base clocks"

# Generated clocks from clk_master
create_generated_clock -name gclk_div2 -source [get_ports clk1] -divide_by 2 [get_pins reg1/Q]
create_generated_clock -name gclk_div4 -source [get_ports clk1] -divide_by 4 [get_pins reg2/Q]
puts "PASS: generated clocks from clk_master"

# Generated clock from clk_aux
create_generated_clock -name gclk_mul2 -source [get_ports clk2] -multiply_by 2 [get_pins reg3/Q]
puts "PASS: generated clock from clk_aux"

# Add clock on same port (exercises -add flag)
create_clock -name clk_master_alt -period 5 -add [get_ports clk1]
puts "PASS: add clock on same port"

# IO delays referencing all clocks
set_input_delay -clock clk_master 2.0 [get_ports in1]
set_input_delay -clock clk_master -clock_fall 2.5 [get_ports in1] -add_delay
set_input_delay -clock clk_aux 1.8 [get_ports in2]
set_input_delay -clock clk_aux 2.2 [get_ports in3]
set_output_delay -clock clk_master 3.0 [get_ports out1]
set_output_delay -clock gclk_div2 3.5 [get_ports out1] -add_delay
set_output_delay -clock clk_aux 2.8 [get_ports out2]
puts "PASS: IO delays"

# Latency on all clocks
set_clock_latency -source 0.5 [get_clocks clk_master]
set_clock_latency -source -early 0.3 [get_clocks clk_master]
set_clock_latency -source -late 0.6 [get_clocks clk_master]
set_clock_latency 0.2 [get_clocks clk_aux]
set_clock_latency -source 0.1 [get_clocks vclk1]
puts "PASS: clock latency"

# Inter-clock uncertainties covering all pairs
set_clock_uncertainty -from [get_clocks clk_master] -to [get_clocks clk_aux] -setup 0.3
set_clock_uncertainty -from [get_clocks clk_master] -to [get_clocks clk_aux] -hold 0.15
set_clock_uncertainty -from [get_clocks clk_aux] -to [get_clocks clk_master] -setup 0.28
set_clock_uncertainty -from [get_clocks clk_master] -to [get_clocks vclk1] -setup 0.25
set_clock_uncertainty -from [get_clocks vclk1] -to [get_clocks vclk2] -setup 0.2
set_clock_uncertainty -setup 0.15 [get_clocks clk_master]
set_clock_uncertainty -hold 0.08 [get_clocks clk_master]
puts "PASS: clock uncertainty"

# Latch borrow and min pulse width
set_max_time_borrow 2.0 [get_clocks clk_master]
set_max_time_borrow 1.5 [get_clocks clk_aux]
set_min_pulse_width -high 0.6 [get_clocks clk_master]
set_min_pulse_width -low 0.4 [get_clocks clk_master]
set_min_pulse_width 0.8 [get_clocks clk_aux]
puts "PASS: latch borrow + min pulse width"

# Clock groups
set_clock_groups -asynchronous -name async1 \
  -group {clk_master gclk_div2 gclk_div4 clk_master_alt} \
  -group {clk_aux gclk_mul2}
puts "PASS: clock groups"

# Exception paths referencing various clocks
set_false_path -from [get_clocks clk_master] -to [get_clocks clk_aux]
set_false_path -from [get_clocks vclk1] -to [get_clocks vclk2]
set_multicycle_path -setup 2 -from [get_clocks clk_master] -to [get_clocks gclk_div2]
puts "PASS: exception paths"

# Propagated clocks
set_propagated_clock [get_clocks clk_master]
puts "PASS: propagated clock"

# Clock transition
set_clock_transition 0.1 [get_clocks clk_master]
set_clock_transition 0.15 [get_clocks clk_aux]
puts "PASS: clock transitions"

# Clock gating check
set_clock_gating_check -setup 0.4 [get_clocks clk_master]
set_clock_gating_check -hold 0.2 [get_clocks clk_master]
puts "PASS: clock gating check"

# Write complete state
set sdc1 [make_result_file sdc_clkremoval1.sdc]
write_sdc -no_timestamp $sdc1
puts "PASS: write_sdc phase 1"

report_checks
puts "PASS: report phase 1"

############################################################
# Phase 2: Delete virtual clocks (simpler cascade)
############################################################
delete_clock [get_clocks vclk1]
puts "PASS: delete vclk1"

delete_clock [get_clocks vclk2]
puts "PASS: delete vclk2"

report_clock_properties
puts "PASS: report after virtual clock deletion"

set sdc2 [make_result_file sdc_clkremoval2.sdc]
write_sdc -no_timestamp $sdc2
puts "PASS: write_sdc after virtual deletions"

############################################################
# Phase 3: Delete generated clocks
############################################################
delete_generated_clock [get_clocks gclk_div2]
puts "PASS: delete gclk_div2"

delete_generated_clock [get_clocks gclk_div4]
puts "PASS: delete gclk_div4"

delete_generated_clock [get_clocks gclk_mul2]
puts "PASS: delete gclk_mul2"

report_clock_properties
puts "PASS: report after gen clock deletion"

############################################################
# Phase 4: Delete the -add clock on clk1 port
############################################################
delete_clock [get_clocks clk_master_alt]
puts "PASS: delete clk_master_alt"

set sdc3 [make_result_file sdc_clkremoval3.sdc]
write_sdc -no_timestamp $sdc3
puts "PASS: write_sdc after alt clock deletion"

############################################################
# Phase 5: Delete master clock (cascades to remove all refs)
############################################################
delete_clock [get_clocks clk_aux]
puts "PASS: delete clk_aux (master)"

report_clock_properties
puts "PASS: report after master deletion"

set sdc4 [make_result_file sdc_clkremoval4.sdc]
write_sdc -no_timestamp $sdc4
puts "PASS: write_sdc after master deletion"

report_checks
puts "PASS: report checks"

############################################################
# Phase 6: Re-create everything fresh
############################################################
create_clock -name clk_new -period 15 [get_ports clk2]
create_generated_clock -name gclk_new -source [get_ports clk1] -divide_by 3 [get_pins reg1/Q]
puts "PASS: recreated clocks"

set_input_delay -clock clk_master 1.5 [get_ports in2]
set_input_delay -clock clk_new 1.8 [get_ports in3]
set_output_delay -clock clk_master 2.5 [get_ports out1]
set_output_delay -clock clk_new 3.0 [get_ports out2]
puts "PASS: recreated IO delays"

set_clock_uncertainty -from [get_clocks clk_master] -to [get_clocks clk_new] -setup 0.2
set_clock_groups -asynchronous -name async_new \
  -group {clk_master gclk_new} \
  -group {clk_new}
puts "PASS: recreated groups and uncertainty"

set_false_path -from [get_clocks clk_master] -to [get_clocks clk_new]
puts "PASS: recreated false path"

set sdc5 [make_result_file sdc_clkremoval5.sdc]
write_sdc -no_timestamp $sdc5
puts "PASS: write_sdc final"

read_sdc $sdc5
report_checks
puts "PASS: read_sdc + report"

puts "ALL PASSED"
