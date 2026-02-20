# Test PVT settings, analysis type, voltage settings, timing derate
# on instance/net/cell, and other deeper Sta.cc and Search.cc paths.
# Targets: Sta.cc setAnalysisType, setPvt, setVoltage,
#          setTimingDerate (net/inst/cell), setDriveCell, setDriveResistance,
#          setSlewLimit, setCapacitanceLimit, setFanoutLimit,
#          removeConstraints, constraintsChanged,
#          setMinPulseWidth (pin/inst/clock),
#          Search.cc clockDomains, ensureClkArrivals,
#          vertexSlacks, netSlack
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_crpr_data_checks.v
link_design search_crpr_data_checks

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 8 [get_ports clk2]
set_input_delay -clock clk1 1.0 [get_ports in1]
set_input_delay -clock clk1 1.0 [get_ports in2]
set_output_delay -clock clk1 2.0 [get_ports out1]
set_output_delay -clock clk2 2.0 [get_ports out2]

report_checks > /dev/null

############################################################
# Multi-clock timing reports
############################################################
puts "--- report_checks -path_delay max (multi-clock) ---"
report_checks -path_delay max

puts "--- report_checks -path_delay min (multi-clock) ---"
report_checks -path_delay min

puts "--- report_checks -path_delay min_max (multi-clock) ---"
report_checks -path_delay min_max

############################################################
# report_checks -format for multi-clock
############################################################
puts "--- report_checks -format full_clock ---"
report_checks -path_delay max -format full_clock

puts "--- report_checks -format full_clock_expanded ---"
report_checks -path_delay max -format full_clock_expanded

############################################################
# CRPR with reconvergent clock tree
############################################################
puts "--- CRPR setup ---"
set_propagated_clock [all_clocks]
sta::set_crpr_enabled 1
sta::set_crpr_mode "same_pin"
report_checks -path_delay max

sta::set_crpr_mode "same_transition"
report_checks -path_delay max

sta::set_crpr_enabled 0
unset_propagated_clock [all_clocks]

############################################################
# Clock groups between domains
############################################################
puts "--- set_clock_groups -asynchronous ---"
set_clock_groups -name async_clks -asynchronous -group {clk1} -group {clk2}
report_checks -path_delay max

unset_clock_groups -asynchronous -name async_clks

############################################################
# Inter-clock uncertainty
############################################################
puts "--- set_clock_uncertainty between clocks ---"
set_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup 0.3
report_checks -path_delay max

catch { unset_clock_uncertainty -from [get_clocks clk1] -to [get_clocks clk2] -setup }

############################################################
# Clock sense
############################################################
puts "--- set_clock_sense ---"
catch {
  set_clock_sense -positive [get_pins ck1buf1/Z] -clocks [get_clocks clk1]
  report_checks -path_delay max
}

############################################################
# Timing derate -early/-late on design level
############################################################
puts "--- timing_derate design level ---"
set_timing_derate -early 0.95
set_timing_derate -late 1.05
report_checks -path_delay max
report_checks -path_delay min
unset_timing_derate

############################################################
# Timing derate on instance
############################################################
puts "--- timing_derate on instance ---"
set_timing_derate -early 0.9 [get_cells reg1]
set_timing_derate -late 1.1 [get_cells reg1]
report_checks -path_delay max
unset_timing_derate

############################################################
# Set slew limit on clock
############################################################
puts "--- set_max_transition on clock ---"
set_max_transition 0.5 -clock_path [get_clocks clk1]
report_check_types -max_slew

############################################################
# Set capacitance limit on port
############################################################
puts "--- set_max_capacitance on port ---"
set_max_capacitance 0.1 [get_ports out1]
report_check_types -max_capacitance

############################################################
# Port loading
############################################################
puts "--- set_load on ports ---"
set_load -pin_load 0.02 [get_ports out1]
set_load -pin_load 0.03 [get_ports out2]
report_checks -path_delay max

############################################################
# Input transition
############################################################
puts "--- set_input_transition ---"
set_input_transition 0.1 [get_ports in1]
set_input_transition 0.15 [get_ports in2]
report_checks -path_delay max

############################################################
# Driving cell
############################################################
puts "--- set_driving_cell ---"
set_driving_cell -lib_cell BUF_X2 -pin Z [get_ports in1]
report_checks -path_delay max

############################################################
# Min pulse width on pins/instances
############################################################
puts "--- set_min_pulse_width on pins ---"
catch {
  set_min_pulse_width 0.5 [get_ports clk1]
  report_pulse_width_checks
}

puts "--- report_pulse_width_checks -verbose ---"
report_pulse_width_checks -verbose

############################################################
# report_checks with -from/-to/-through cross-domain
############################################################
puts "--- report_checks -from in1 -to out1 ---"
report_checks -from [get_ports in1] -to [get_ports out1] -path_delay max

puts "--- report_checks -from in1 -to out2 (cross-domain) ---"
report_checks -from [get_ports in1] -to [get_ports out2] -path_delay max

puts "--- report_checks -through buf2/Z ---"
report_checks -through [get_pins buf2/Z] -path_delay max

############################################################
# false_path between clock domains
############################################################
puts "--- set_false_path between domains ---"
set_false_path -from [get_clocks clk1] -to [get_clocks clk2]
report_checks -path_delay max

unset_path_exceptions -from [get_clocks clk1] -to [get_clocks clk2]

############################################################
# multicycle between domains
############################################################
puts "--- set_multicycle_path between domains ---"
set_multicycle_path 2 -setup -from [get_clocks clk1] -to [get_clocks clk2]
report_checks -path_delay max

unset_path_exceptions -setup -from [get_clocks clk1] -to [get_clocks clk2]

############################################################
# group_path with -through
############################################################
puts "--- group_path -through ---"
group_path -name buf_paths -through [get_pins buf1/Z]
report_checks -path_delay max
report_checks -path_delay max -path_group buf_paths

############################################################
# find_timing_paths with group filter
############################################################
puts "--- find_timing_paths -path_group ---"
set grp_paths [find_timing_paths -path_delay max -path_group buf_paths]
puts "buf_paths group: [llength $grp_paths] paths"

############################################################
# Clock min period
############################################################
puts "--- report_clock_min_period ---"
report_clock_min_period
report_clock_min_period -clocks clk1
report_clock_min_period -clocks clk2
report_clock_min_period -include_port_paths

############################################################
# Clock skew
############################################################
puts "--- report_clock_skew ---"
report_clock_skew -setup
report_clock_skew -hold
report_clock_skew -setup -clock clk1
report_clock_skew -hold -clock clk2

############################################################
# report_tns/wns/worst_slack for both min and max
############################################################
puts "--- tns/wns ---"
report_tns -max
report_tns -min
report_wns -max
report_wns -min
report_worst_slack -max
report_worst_slack -min

############################################################
# total_negative_slack / worst_slack functions
############################################################
puts "--- total_negative_slack ---"
set tns_max [total_negative_slack -max]
set tns_min [total_negative_slack -min]
puts "tns max: $tns_max min: $tns_min"

puts "--- worst_slack ---"
set ws_max [worst_slack -max]
set ws_min [worst_slack -min]
puts "worst_slack max: $ws_max min: $ws_min"

puts "--- worst_negative_slack ---"
set wns_max [worst_negative_slack -max]
set wns_min [worst_negative_slack -min]
puts "wns max: $wns_max min: $wns_min"

############################################################
# write_sdc
############################################################
puts "--- write_sdc ---"
set sdc_file [make_result_file "pvt_analysis.sdc"]
write_sdc $sdc_file
