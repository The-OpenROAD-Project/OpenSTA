# Test SDC constraint creation, removal, and re-creation
# Targets: Sdc.cc (removeClock, deleteMasterClkRefs, removeInputDelay, removeOutputDelay,
#          deleteInputDelaysReferencing, deleteOutputDelaysReferencing,
#          deleteClockLatency, deleteClockInsertion, deleteInterClockUncertainty,
#          deleteClockInsertionsReferencing, deleteMinPulseWidthReferencing,
#          deleteLatchBorrowLimitsReferencing, clockGroupsDeleteClkRefs),
#          ExceptionPath.cc (exception removal, priority comparison),
#          Clock.cc (initClk, deletion, re-creation),
#          WriteSdc.cc (writing after removals),
#          CycleAccting.cc (cycle accounting with different clock configs)
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

############################################################
# Phase 1: Create comprehensive constraints
############################################################

# Clocks
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 -waveform {0 10} [get_ports clk2]
create_clock -name vclk -period 8
puts "PASS: Phase 1 - clocks"

# Generated clocks
create_generated_clock -name gclk1 -source [get_ports clk1] -divide_by 2 [get_pins reg1/Q]
create_generated_clock -name gclk2 -source [get_ports clk2] -multiply_by 2 [get_pins reg3/Q]
puts "PASS: Phase 1 - generated clocks"

# IO delays
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 -rise -max 2.5 [get_ports in2]
set_input_delay -clock clk1 -fall -min 1.0 [get_ports in2]
set_input_delay -clock clk2 1.8 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 3.5 [get_ports out2]
puts "PASS: Phase 1 - IO delays"

# Clock latency
set_clock_latency -source 0.5 [get_clocks clk1]
set_clock_latency -source -rise -max 0.6 [get_clocks clk1]
set_clock_latency 0.2 [get_clocks clk2]
puts "PASS: Phase 1 - clock latency"

# Clock uncertainty
set_clock_uncertainty -setup 0.2 [get_clocks clk1]
set_clock_uncertainty -hold 0.1 [get_clocks clk1]
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup 0.3
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -hold 0.15
puts "PASS: Phase 1 - clock uncertainty"

# Clock transition
set_clock_transition 0.1 [get_clocks clk1]
set_clock_transition 0.15 [get_clocks clk2]
puts "PASS: Phase 1 - clock transition"

# Source latency with early/late
set_clock_latency -source -early 0.3 [get_clocks clk1]
set_clock_latency -source -late 0.5 [get_clocks clk1]
puts "PASS: Phase 1 - source latency early/late"

# Min pulse width
set_min_pulse_width 1.0 [get_clocks clk1]
set_min_pulse_width -high 0.6 [get_clocks clk2]
puts "PASS: Phase 1 - min pulse width"

# Latch borrow limit
set_max_time_borrow 2.0 [get_clocks clk1]
set_max_time_borrow 1.5 [get_pins reg1/D]
puts "PASS: Phase 1 - latch borrow"

# Clock groups
set_clock_groups -asynchronous -name grp1 \
  -group {clk1 gclk1} \
  -group {clk2 gclk2}
puts "PASS: Phase 1 - clock groups"

# Exception paths
set_false_path -from [get_clocks clk1] -to [get_clocks clk2]
set_multicycle_path -setup 2 -from [get_ports in1] -to [get_ports out1]
set_multicycle_path -hold 1 -from [get_ports in1] -to [get_ports out1]
set_max_delay -from [get_ports in2] -to [get_ports out1] 8.0
set_min_delay -from [get_ports in2] -to [get_ports out1] 1.0
group_path -name grp_io -from [get_ports {in1 in2}] -to [get_ports out1]
puts "PASS: Phase 1 - exception paths"

# Timing derate
set_timing_derate -early 0.95
set_timing_derate -late 1.05
puts "PASS: Phase 1 - timing derate"

# Disable timing
set_disable_timing [get_cells buf1]
puts "PASS: Phase 1 - disable timing"

# Case analysis
set_case_analysis 0 [get_ports in3]
puts "PASS: Phase 1 - case analysis"

# Design limits
set_max_transition 0.5 [current_design]
set_max_capacitance 0.2 [current_design]
set_max_fanout 20 [current_design]
set_max_area 100.0
puts "PASS: Phase 1 - design limits"

# Write Phase 1
set sdc_phase1 [make_result_file sdc_removal_phase1.sdc]
write_sdc -no_timestamp $sdc_phase1
puts "PASS: Phase 1 - write_sdc"

report_checks
puts "PASS: Phase 1 - report_checks"

############################################################
# Phase 2: Remove constraints systematically
############################################################

# Remove exceptions
unset_path_exceptions -from [get_clocks clk1] -to [get_clocks clk2]
puts "PASS: Phase 2 - remove false path"

unset_path_exceptions -setup -from [get_ports in1] -to [get_ports out1]
unset_path_exceptions -hold -from [get_ports in1] -to [get_ports out1]
puts "PASS: Phase 2 - remove multicycle"

# Remove timing derate
unset_timing_derate
puts "PASS: Phase 2 - remove timing derate"

# Remove disable timing
unset_disable_timing [get_cells buf1]
puts "PASS: Phase 2 - remove disable timing"

# Remove case analysis
unset_case_analysis [get_ports in3]
puts "PASS: Phase 2 - remove case analysis"

# Remove clock groups
unset_clock_groups -asynchronous -name grp1
puts "PASS: Phase 2 - remove clock groups"

# Remove clock latency
unset_clock_latency [get_clocks clk1]
unset_clock_latency -source [get_clocks clk1]
unset_clock_latency [get_clocks clk2]
puts "PASS: Phase 2 - remove clock latency"

# Remove inter-clock uncertainty
unset_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup
unset_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -hold
puts "PASS: Phase 2 - remove inter-clock uncertainty"

# Remove propagated clock
set_propagated_clock [get_clocks clk1]
unset_propagated_clock [get_clocks clk1]
puts "PASS: Phase 2 - remove propagated clock"

# Write Phase 2 (many constraints removed)
set sdc_phase2 [make_result_file sdc_removal_phase2.sdc]
write_sdc -no_timestamp $sdc_phase2
puts "PASS: Phase 2 - write_sdc"

report_checks
puts "PASS: Phase 2 - report_checks"

############################################################
# Phase 3: Delete and re-create clocks
# (this is the key test - deleting clocks should remove
#  all referencing constraints)
############################################################

# Delete generated clocks first
delete_generated_clock [get_clocks gclk1]
puts "PASS: Phase 3 - delete gclk1"

delete_generated_clock [get_clocks gclk2]
puts "PASS: Phase 3 - delete gclk2"

# Delete virtual clock
delete_clock [get_clocks vclk]
puts "PASS: Phase 3 - delete vclk"

report_clock_properties
puts "PASS: Phase 3 - report after clock deletions"

# Write after clock deletions
set sdc_phase3a [make_result_file sdc_removal_phase3a.sdc]
write_sdc -no_timestamp $sdc_phase3a
puts "PASS: Phase 3a - write_sdc"

report_checks
puts "PASS: Phase 3a - report_checks"

############################################################
# Phase 4: Re-create everything fresh
############################################################

# Re-create virtual clock with different period
create_clock -name vclk_new -period 12
puts "PASS: Phase 4 - new virtual clock"

# Re-create generated clocks on new sources
create_generated_clock -name gclk_new1 -source [get_ports clk1] -divide_by 4 [get_pins reg1/Q]
puts "PASS: Phase 4 - new generated clock"

# New clock groups
set_clock_groups -asynchronous -name new_grp \
  -group {clk1 gclk_new1} \
  -group {clk2}
puts "PASS: Phase 4 - new clock groups"

# New exceptions
set_false_path -from [get_clocks clk1] -to [get_clocks clk2]
set_multicycle_path -setup 3 -from [get_ports in2] -to [get_ports out2]
puts "PASS: Phase 4 - new exceptions"

# New latency
set_clock_latency -source 0.4 [get_clocks clk1]
set_clock_latency 0.15 [get_clocks clk2]
puts "PASS: Phase 4 - new latency"

# New uncertainty
set_clock_uncertainty -setup 0.25 [get_clocks clk1]
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup 0.35
puts "PASS: Phase 4 - new uncertainty"

# New derate
set_timing_derate -early 0.96
set_timing_derate -late 1.04
puts "PASS: Phase 4 - new derate"

# New disable
set_disable_timing [get_cells inv1]
puts "PASS: Phase 4 - new disable"

# Write Phase 4
set sdc_phase4 [make_result_file sdc_removal_phase4.sdc]
write_sdc -no_timestamp $sdc_phase4
puts "PASS: Phase 4 - write_sdc"

set sdc_phase4_compat [make_result_file sdc_removal_phase4_compat.sdc]
write_sdc -no_timestamp -compatible $sdc_phase4_compat
puts "PASS: Phase 4 - write_sdc compatible"

report_checks
puts "PASS: Phase 4 - report_checks"

############################################################
# Phase 5: Read back SDC and verify
############################################################

read_sdc $sdc_phase4
puts "PASS: Phase 5 - read_sdc"

report_checks
puts "PASS: Phase 5 - report_checks after read"

set sdc_phase5 [make_result_file sdc_removal_phase5.sdc]
write_sdc -no_timestamp $sdc_phase5
puts "PASS: Phase 5 - write_sdc after read"

puts "ALL PASSED"
