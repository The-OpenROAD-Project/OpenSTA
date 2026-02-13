# Test advanced exception path features for code coverage
# Targets: ExceptionPath.cc (priority, comparison, rise/fall/through combinations),
#          WriteSdc.cc (writeException, writeExceptionFrom/To/Thru),
#          Sdc.cc (exception management, matching)
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

############################################################
# Setup clocks and basic delays
############################################################

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_input_delay -clock clk2 2.0 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]
puts "PASS: basic setup"

############################################################
# False path with rise/fall from/to combinations
############################################################

# rise_from to specific port
set_false_path -rise_from [get_ports in1] -to [get_ports out1]
puts "PASS: false_path -rise_from"

# fall_from to specific port
set_false_path -fall_from [get_ports in2] -to [get_ports out1]
puts "PASS: false_path -fall_from"

# from port to rise_to
set_false_path -from [get_ports in1] -rise_to [get_ports out2]
puts "PASS: false_path -rise_to"

# from port to fall_to
set_false_path -from [get_ports in2] -fall_to [get_ports out2]
puts "PASS: false_path -fall_to"

# rise_from + fall_to combination
set_false_path -rise_from [get_ports in3] -fall_to [get_ports out1]
puts "PASS: false_path -rise_from -fall_to"

# fall_from + rise_to combination
set_false_path -fall_from [get_ports in3] -rise_to [get_ports out2]
puts "PASS: false_path -fall_from -rise_to"

# Report after false paths
report_checks
puts "PASS: report after rise/fall false paths"

############################################################
# Write SDC (to cover exception writing with rise/fall)
############################################################

set sdc_file1 [make_result_file sdc_exception_adv1.sdc]
write_sdc -no_timestamp $sdc_file1
puts "PASS: write_sdc after false paths"

############################################################
# Unset all false paths and create through paths
############################################################

unset_path_exceptions -rise_from [get_ports in1] -to [get_ports out1]
unset_path_exceptions -fall_from [get_ports in2] -to [get_ports out1]
unset_path_exceptions -from [get_ports in1] -rise_to [get_ports out2]
unset_path_exceptions -from [get_ports in2] -fall_to [get_ports out2]
unset_path_exceptions -rise_from [get_ports in3] -fall_to [get_ports out1]
unset_path_exceptions -fall_from [get_ports in3] -rise_to [get_ports out2]
puts "PASS: unset all false paths"

############################################################
# False path with -through
############################################################

set_false_path -from [get_ports in1] -through [get_pins buf1/Z] -to [get_ports out1]
puts "PASS: false_path with -through pin"

set_false_path -from [get_ports in2] -through [get_pins inv1/ZN] -through [get_pins and1/ZN] -to [get_ports out1]
puts "PASS: false_path with multiple -through"

set_false_path -from [get_ports in1] -through [get_pins or1/ZN] -to [get_ports out2]
puts "PASS: false_path -through to out2"

# Write to cover through paths
set sdc_file2 [make_result_file sdc_exception_adv2.sdc]
write_sdc -no_timestamp $sdc_file2
puts "PASS: write_sdc with through paths"

# Unset through paths
unset_path_exceptions -from [get_ports in1] -through [get_pins buf1/Z] -to [get_ports out1]
unset_path_exceptions -from [get_ports in2] -through [get_pins inv1/ZN] -through [get_pins and1/ZN] -to [get_ports out1]
unset_path_exceptions -from [get_ports in1] -through [get_pins or1/ZN] -to [get_ports out2]
puts "PASS: unset through paths"

############################################################
# Multicycle path with rise/fall combinations
############################################################

# Setup multicycle with rise_from
set_multicycle_path -setup 2 -rise_from [get_ports in1] -to [get_ports out1]
puts "PASS: multicycle -rise_from"

# Hold multicycle with fall_to
set_multicycle_path -hold 1 -from [get_ports in1] -fall_to [get_ports out1]
puts "PASS: multicycle -fall_to"

# Multicycle between clock domains
set_multicycle_path -setup 3 -from [get_clocks clk1] -to [get_clocks clk2]
puts "PASS: multicycle clk1->clk2"

set_multicycle_path -hold 2 -from [get_clocks clk1] -to [get_clocks clk2]
puts "PASS: multicycle hold clk1->clk2"

# Multicycle with -through
set_multicycle_path -setup 4 -from [get_ports in2] -through [get_pins and1/ZN] -to [get_ports out1]
puts "PASS: multicycle with -through"

# Write to cover multicycle writing
set sdc_file3 [make_result_file sdc_exception_adv3.sdc]
write_sdc -no_timestamp $sdc_file3
puts "PASS: write_sdc with multicycle"

# Report
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: report_checks with multicycle"

############################################################
# Unset multicycles and add max/min delay
############################################################

unset_path_exceptions -setup -rise_from [get_ports in1] -to [get_ports out1]
unset_path_exceptions -hold -from [get_ports in1] -fall_to [get_ports out1]
unset_path_exceptions -setup -from [get_clocks clk1] -to [get_clocks clk2]
unset_path_exceptions -hold -from [get_clocks clk1] -to [get_clocks clk2]
unset_path_exceptions -setup -from [get_ports in2] -through [get_pins and1/ZN] -to [get_ports out1]
puts "PASS: unset multicycles"

############################################################
# Max/min delay with various options
############################################################

set_max_delay -from [get_ports in1] -to [get_ports out1] 8.0
set_min_delay -from [get_ports in1] -to [get_ports out1] 1.0
puts "PASS: max/min delay"

set_max_delay -from [get_ports in2] -through [get_pins and1/ZN] -to [get_ports out1] 6.0
puts "PASS: max_delay with -through"

set_max_delay -rise_from [get_ports in3] -fall_to [get_ports out2] 7.0
puts "PASS: max_delay -rise_from -fall_to"

# Write to cover max/min delay writing
set sdc_file4 [make_result_file sdc_exception_adv4.sdc]
write_sdc -no_timestamp $sdc_file4
puts "PASS: write_sdc with max/min delay"

# Write compatible mode to cover alternative writer paths
set sdc_file5 [make_result_file sdc_exception_adv5.sdc]
write_sdc -no_timestamp -compatible $sdc_file5
puts "PASS: write_sdc -compatible with exceptions"

# Write with digits for coverage
set sdc_file6 [make_result_file sdc_exception_adv6.sdc]
write_sdc -no_timestamp -digits 6 $sdc_file6
puts "PASS: write_sdc -digits 6 with exceptions"

############################################################
# Group paths (exercises group_path writing)
############################################################

group_path -name reg2reg -from [get_clocks clk1] -to [get_clocks clk1]
group_path -name in2out -from [get_ports {in1 in2 in3}] -to [get_ports {out1 out2}]
group_path -name clk_cross -from [get_clocks clk1] -to [get_clocks clk2]
puts "PASS: group_path"

report_checks -path_group reg2reg
puts "PASS: report_checks -path_group reg2reg"

report_checks -path_group in2out
puts "PASS: report_checks -path_group in2out"

# Write with group paths
set sdc_file7 [make_result_file sdc_exception_adv7.sdc]
write_sdc -no_timestamp $sdc_file7
puts "PASS: write_sdc with group paths"

############################################################
# Read back SDC to verify
############################################################

read_sdc $sdc_file4
puts "PASS: read_sdc"

report_checks
puts "PASS: report after read_sdc"

puts "ALL PASSED"
