# Test CycleAccting.cc: cycle accounting with clocks of different periods,
# multicycle paths (-start/-end), half-period clocks, and clock relationships.
# Exercises CycleAccting.cc findDelays with various src/tgt clock edge combos,
# findDefaultArrivalSrcDelays, requiredTime, sourceTimeOffset, targetTimeOffset,
# maxCycles, reportClkToClkMaxCycleWarnings.
# Also exercises Sdc.cc multicyclePath, set_multicycle_path -start/-end,
# Clock.cc edge operations with different period clocks.
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

############################################################
# Phase 1: Clocks with different periods for cycle accounting
############################################################
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 15 [get_ports clk2]

set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_input_delay -clock clk2 3.0 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 4.0 [get_ports out2]

############################################################
# Phase 2: Multicycle path -setup (default -end)
############################################################
puts "--- multicycle -setup 2 ---"
set_multicycle_path -setup 2 -from [get_clocks clk1] -to [get_clocks clk2]
report_checks -path_delay max

puts "--- multicycle -hold 1 ---"
set_multicycle_path -hold 1 -from [get_clocks clk1] -to [get_clocks clk2]
report_checks -path_delay min

############################################################
# Phase 3: Multicycle with -start
############################################################
puts "--- multicycle -setup 3 -start ---"
set_multicycle_path -setup 3 -start -from [get_clocks clk1] -to [get_clocks clk2]
report_checks -path_delay max -from [get_ports in1] -to [get_ports out2]

puts "--- multicycle -hold 2 -start ---"
set_multicycle_path -hold 2 -start -from [get_clocks clk1] -to [get_clocks clk2]
report_checks -path_delay min -from [get_ports in1] -to [get_ports out2]

############################################################
# Phase 4: Multicycle with -end
############################################################
puts "--- multicycle -setup 4 -end ---"
set_multicycle_path -setup 4 -end -from [get_clocks clk2] -to [get_clocks clk1]
report_checks -path_delay max -from [get_ports in3] -to [get_ports out1]

puts "--- multicycle -hold 3 -end ---"
set_multicycle_path -hold 3 -end -from [get_clocks clk2] -to [get_clocks clk1]
report_checks -path_delay min -from [get_ports in3] -to [get_ports out1]

############################################################
# Phase 5: Unset and re-do multicycle
############################################################
puts "--- unset_path_exceptions ---"
unset_path_exceptions -setup -from [get_clocks clk1] -to [get_clocks clk2]
unset_path_exceptions -hold -from [get_clocks clk1] -to [get_clocks clk2]
unset_path_exceptions -setup -from [get_clocks clk2] -to [get_clocks clk1]
unset_path_exceptions -hold -from [get_clocks clk2] -to [get_clocks clk1]
report_checks -path_delay max

############################################################
# Phase 6: Same clock domain multicycle
############################################################
puts "--- same domain multicycle ---"
set_multicycle_path -setup 2 -from [get_clocks clk1] -to [get_clocks clk1]
set_multicycle_path -hold 1 -from [get_clocks clk1] -to [get_clocks clk1]
report_checks -path_delay max
report_checks -path_delay min

############################################################
# Phase 7: Re-create clocks with non-integer ratio periods
############################################################
puts "--- reclk with non-integer ratio ---"
delete_clock [get_clocks clk1]
delete_clock [get_clocks clk2]

create_clock -name clk_a -period 7 [get_ports clk1]
create_clock -name clk_b -period 11 [get_ports clk2]

set_input_delay -clock clk_a 1.0 [get_ports in1]
set_input_delay -clock clk_a 1.0 [get_ports in2]
set_input_delay -clock clk_b 2.0 [get_ports in3]
set_output_delay -clock clk_a 2.0 [get_ports out1]
set_output_delay -clock clk_b 3.0 [get_ports out2]

report_checks -path_delay max
report_checks -path_delay min

puts "--- multicycle on non-integer ratio ---"
set_multicycle_path -setup 2 -from [get_clocks clk_a] -to [get_clocks clk_b]
report_checks -path_delay max

############################################################
# Phase 8: Half-period clock (waveform test)
############################################################
puts "--- half-period waveform ---"
delete_clock [get_clocks clk_a]
delete_clock [get_clocks clk_b]

create_clock -name clk_half -period 10 -waveform {0 3} [get_ports clk1]
create_clock -name clk_norm -period 10 [get_ports clk2]

set_input_delay -clock clk_half 1.0 [get_ports in1]
set_input_delay -clock clk_half 1.0 [get_ports in2]
set_input_delay -clock clk_norm 1.0 [get_ports in3]
set_output_delay -clock clk_half 2.0 [get_ports out1]
set_output_delay -clock clk_norm 2.0 [get_ports out2]

report_checks -path_delay max
report_checks -path_delay min

puts "--- multicycle half-period ---"
set_multicycle_path -setup 2 -from [get_clocks clk_half] -to [get_clocks clk_norm]
set_multicycle_path -hold 1 -from [get_clocks clk_half] -to [get_clocks clk_norm]
report_checks -path_delay max
report_checks -path_delay min

############################################################
# Phase 9: Write SDC
############################################################
set sdc_out [make_result_file sdc_cycle_acct.sdc]
write_sdc -no_timestamp $sdc_out
diff_files sdc_cycle_acct.sdcok $sdc_out

############################################################
# Phase 10: report_clock_properties
############################################################
puts "--- report_clock_properties ---"
report_clock_properties
