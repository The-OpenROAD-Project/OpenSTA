# Test exception path merging, priority resolution, through-pin matching,
# complex false/multicycle/max_delay/min_delay/group_path combinations.
# Targets: ExceptionPath.cc exception merging, priority, matches,
#          overrides, through-pin matching, ExceptionThru matching,
#          Sdc.cc addException, findException, isPathGroupName,
#          pathGroupNames, removeConstraints, constraintsChanged,
#          makeExceptionFrom/Thru/To, checkExceptionFromPins,
#          checkExceptionToPins, deleteExceptionFrom/Thru/To,
#          WriteSdc.cc writeExceptions (various exception types)
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_input_delay -clock clk2 2.0 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]

############################################################
# Simple false path
############################################################
puts "--- false path clock to clock ---"
set_false_path -from [get_clocks clk1] -to [get_clocks clk2]
report_checks

############################################################
# False path with rise_from/fall_to
############################################################
puts "--- false path rise_from/fall_to ---"
set_false_path -rise_from [get_ports in1] -fall_to [get_ports out1]
report_checks

puts "--- false path fall_from/rise_to ---"
set_false_path -fall_from [get_ports in2] -rise_to [get_ports out2]
report_checks

############################################################
# False path with -through
############################################################
puts "--- false path through single pin ---"
set_false_path -from [get_ports in1] -through [get_pins and1/ZN] -to [get_ports out1]
report_checks

puts "--- false path through instance pin ---"
set_false_path -from [get_ports in2] -through [get_pins inv1/ZN]
report_checks

puts "--- false path through second pin ---"
set_false_path -from [get_ports in1] -through [get_pins buf1/Z] -to [get_ports out2]
report_checks

############################################################
# Multicycle paths with various options
############################################################
puts "--- multicycle setup ---"
set_multicycle_path -setup 2 -from [get_ports in1] -to [get_ports out1]
report_checks

puts "--- multicycle hold ---"
set_multicycle_path -hold 1 -from [get_ports in1] -to [get_ports out1]
report_checks

puts "--- multicycle with -start ---"
set_multicycle_path -setup 3 -start -from [get_clocks clk1] -to [get_clocks clk2]
report_checks

puts "--- multicycle with -end ---"
set_multicycle_path -setup 2 -end -from [get_clocks clk1]
report_checks

puts "--- multicycle with rise_from ---"
set_multicycle_path -setup 4 -rise_from [get_ports in1]
report_checks

puts "--- multicycle with fall_to ---"
set_multicycle_path -hold 2 -fall_to [get_ports out1]
report_checks

############################################################
# Max/min delay constraints
############################################################
puts "--- max_delay ---"
set_max_delay -from [get_ports in1] -to [get_ports out1] 8.0
report_checks -path_delay max

puts "--- min_delay ---"
set_min_delay -from [get_ports in1] -to [get_ports out1] 1.0
report_checks -path_delay min

puts "--- max_delay with through ---"
set_max_delay -from [get_ports in2] -through [get_pins inv1/ZN] -to [get_ports out1] 6.0
report_checks -path_delay max

puts "--- min_delay with through ---"
set_min_delay -from [get_ports in2] -through [get_pins inv1/ZN] -to [get_ports out1] 0.5
report_checks -path_delay min

puts "--- max_delay rise_from ---"
set_max_delay -rise_from [get_ports in3] -to [get_ports out2] 7.0
report_checks -path_delay max

############################################################
# Group paths
############################################################
puts "--- group_path ---"
group_path -name grp1 -from [get_clocks clk1]
group_path -name grp2 -from [get_ports in1] -to [get_ports out1]
group_path -name grp3 -from [get_clocks clk2] -to [get_ports out2]
report_checks

puts "--- path group names ---"
set pgn [sta::path_group_names]
puts "Path group names: $pgn"

puts "--- is_path_group_name ---"
puts "grp1 is group: [sta::is_path_group_name grp1]"
puts "nonexistent is group: [sta::is_path_group_name nonexistent]"

############################################################
# Exception priority and overriding
############################################################
puts "--- exception override: false path then max_delay ---"
# More specific exception should override broader one
set_max_delay -from [get_ports in3] -to [get_ports out2] 5.0
report_checks

############################################################
# remove_constraints (remove all SDC constraints)
############################################################
puts "--- remove_constraints ---"
sta::remove_constraints
report_checks

# Re-add constraints for write_sdc
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_input_delay -clock clk2 2.0 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]

set_false_path -from [get_clocks clk1] -to [get_clocks clk2]
set_multicycle_path -setup 2 -from [get_ports in1] -to [get_ports out1]
set_max_delay -from [get_ports in2] -to [get_ports out1] 8.0
group_path -name grp1 -from [get_clocks clk1]
report_checks

############################################################
# Write SDC with all exception types
############################################################
puts "--- write_sdc with exceptions ---"
set sdc1 [make_result_file sdc_exception_merge1.sdc]
write_sdc -no_timestamp $sdc1

puts "--- write_sdc compatible with exceptions ---"
set sdc2 [make_result_file sdc_exception_merge2.sdc]
write_sdc -no_timestamp -compatible $sdc2

############################################################
# Read back and verify
############################################################
puts "--- read_sdc back ---"
read_sdc $sdc1
report_checks

############################################################
# Additional write after re-read
############################################################
set sdc3 [make_result_file sdc_exception_merge3.sdc]
write_sdc -no_timestamp $sdc3
