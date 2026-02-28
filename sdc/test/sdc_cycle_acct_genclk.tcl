# Test CycleAccting.cc with generated clocks, odd period ratios,
# and waveform offsets that stress the cycle accounting convergence loop.
# Targets: CycleAccting.cc findDelays with non-trivial period ratios,
#   firstCycle with edge times, setSetupAccting, setHoldAccting,
#   setAccting for latchSetup/latchHold/gatedClockSetup/gatedClockHold,
#   findDefaultArrivalSrcDelays, setDefaultSetupAccting, setDefaultHoldAccting,
#   maxCyclesExceeded, reportClkToClkMaxCycleWarnings,
#   CycleAcctingLess, CycleAcctingHash, CycleAcctingEqual,
#   requiredTime, sourceTimeOffset, targetTimeOffset
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

############################################################
# Phase 1: Generated clock from master clock
############################################################
create_clock -name master -period 10 [get_ports clk1]
create_generated_clock -name gen_div2 -source [get_ports clk1] \
  -divide_by 2 [get_ports clk2]

set_input_delay -clock master 1.0 [get_ports in1]
set_input_delay -clock master 1.0 [get_ports in2]
set_input_delay -clock gen_div2 2.0 [get_ports in3]
set_output_delay -clock master 2.0 [get_ports out1]
set_output_delay -clock gen_div2 3.0 [get_ports out2]

report_checks -path_delay max
report_checks -path_delay min

############################################################
# Phase 2: Multicycle on generated clock paths
############################################################
puts "--- mcp setup 2 master -> gen_div2 ---"
set_multicycle_path -setup 2 -from [get_clocks master] -to [get_clocks gen_div2]
report_checks -path_delay max -from [get_ports in1] -to [get_ports out2]

puts "--- mcp hold 1 master -> gen_div2 ---"
set_multicycle_path -hold 1 -from [get_clocks master] -to [get_clocks gen_div2]
report_checks -path_delay min -from [get_ports in1] -to [get_ports out2]

puts "--- mcp setup 2 gen_div2 -> master ---"
set_multicycle_path -setup 2 -from [get_clocks gen_div2] -to [get_clocks master]
report_checks -path_delay max -from [get_ports in3] -to [get_ports out1]

unset_path_exceptions -setup -from [get_clocks master] -to [get_clocks gen_div2]
unset_path_exceptions -hold -from [get_clocks master] -to [get_clocks gen_div2]
unset_path_exceptions -setup -from [get_clocks gen_div2] -to [get_clocks master]

############################################################
# Phase 3: Delete and create clocks with odd ratios
# Exercises findDelays with non-convergent cycles
############################################################
puts "--- odd ratio clocks ---"
delete_clock [get_clocks master]
delete_clock [get_clocks gen_div2]

create_clock -name clk_p7 -period 7 [get_ports clk1]
create_clock -name clk_p13 -period 13 [get_ports clk2]

set_input_delay -clock clk_p7 1.0 [get_ports in1]
set_input_delay -clock clk_p7 1.0 [get_ports in2]
set_input_delay -clock clk_p13 2.0 [get_ports in3]
set_output_delay -clock clk_p7 2.0 [get_ports out1]
set_output_delay -clock clk_p13 3.0 [get_ports out2]

report_checks -path_delay max
report_checks -path_delay min

puts "--- mcp on odd ratio ---"
set_multicycle_path -setup 3 -from [get_clocks clk_p7] -to [get_clocks clk_p13]
set_multicycle_path -hold 2 -from [get_clocks clk_p7] -to [get_clocks clk_p13]
report_checks -path_delay max
report_checks -path_delay min

############################################################
# Phase 4: Waveform with edge offset
############################################################
puts "--- waveform edge offset ---"
delete_clock [get_clocks clk_p7]
delete_clock [get_clocks clk_p13]

# Clock with offset waveform (exercises firstCycle edge time handling)
create_clock -name clk_off -period 10 -waveform {2 7} [get_ports clk1]
create_clock -name clk_norm -period 10 [get_ports clk2]

set_input_delay -clock clk_off 1.0 [get_ports in1]
set_input_delay -clock clk_off 1.0 [get_ports in2]
set_input_delay -clock clk_norm 1.0 [get_ports in3]
set_output_delay -clock clk_off 2.0 [get_ports out1]
set_output_delay -clock clk_norm 2.0 [get_ports out2]

report_checks -path_delay max
report_checks -path_delay min

puts "--- mcp waveform edge offset ---"
set_multicycle_path -setup 2 -from [get_clocks clk_off] -to [get_clocks clk_norm]
set_multicycle_path -hold 1 -from [get_clocks clk_off] -to [get_clocks clk_norm]
report_checks -path_delay max
report_checks -path_delay min

############################################################
# Phase 5: Generated clock with multiply_by
############################################################
puts "--- generated clock multiply_by ---"
delete_clock [get_clocks clk_off]
delete_clock [get_clocks clk_norm]

create_clock -name base -period 20 [get_ports clk1]
create_generated_clock -name gen_mult -source [get_ports clk1] \
  -multiply_by 2 [get_ports clk2]

set_input_delay -clock base 2.0 [get_ports in1]
set_input_delay -clock base 2.0 [get_ports in2]
set_input_delay -clock gen_mult 1.0 [get_ports in3]
set_output_delay -clock base 3.0 [get_ports out1]
set_output_delay -clock gen_mult 2.0 [get_ports out2]

report_checks -path_delay max
report_checks -path_delay min

puts "--- mcp base -> gen_mult ---"
set_multicycle_path -setup 2 -from [get_clocks base] -to [get_clocks gen_mult]
report_checks -path_delay max

############################################################
# Phase 6: Generated clock with edge list
############################################################
puts "--- generated clock edge list ---"
delete_clock [get_clocks base]
delete_clock [get_clocks gen_mult]

create_clock -name mclk -period 10 [get_ports clk1]
create_generated_clock -name edge_clk -source [get_ports clk1] \
  -edges {1 3 5} [get_ports clk2]

set_input_delay -clock mclk 1.0 [get_ports in1]
set_input_delay -clock mclk 1.0 [get_ports in2]
set_input_delay -clock edge_clk 1.5 [get_ports in3]
set_output_delay -clock mclk 2.0 [get_ports out1]
set_output_delay -clock edge_clk 2.5 [get_ports out2]

report_checks -path_delay max
report_checks -path_delay min

############################################################
# Phase 7: report_clock_properties
############################################################
puts "--- report_clock_properties ---"
report_clock_properties

############################################################
# Phase 8: Write SDC roundtrip
############################################################
set sdc_out [make_result_file sdc_cycle_acct_genclk.sdc]
write_sdc -no_timestamp $sdc_out
diff_files sdc_cycle_acct_genclk.sdcok $sdc_out
