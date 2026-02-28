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

############################################################
# False path with rise/fall from/to combinations
############################################################

# rise_from to specific port
set_false_path -rise_from [get_ports in1] -to [get_ports out1]

# fall_from to specific port
set_false_path -fall_from [get_ports in2] -to [get_ports out1]

# from port to rise_to
set_false_path -from [get_ports in1] -rise_to [get_ports out2]

# from port to fall_to
set_false_path -from [get_ports in2] -fall_to [get_ports out2]

# rise_from + fall_to combination
set_false_path -rise_from [get_ports in3] -fall_to [get_ports out1]

# fall_from + rise_to combination
set_false_path -fall_from [get_ports in3] -rise_to [get_ports out2]

# Report after false paths
report_checks

############################################################
# Write SDC (to cover exception writing with rise/fall)
############################################################

set sdc_file1 [make_result_file sdc_exception_adv1.sdc]
write_sdc -no_timestamp $sdc_file1

############################################################
# Unset all false paths and create through paths
############################################################

unset_path_exceptions -rise_from [get_ports in1] -to [get_ports out1]
unset_path_exceptions -fall_from [get_ports in2] -to [get_ports out1]
unset_path_exceptions -from [get_ports in1] -rise_to [get_ports out2]
unset_path_exceptions -from [get_ports in2] -fall_to [get_ports out2]
unset_path_exceptions -rise_from [get_ports in3] -fall_to [get_ports out1]
unset_path_exceptions -fall_from [get_ports in3] -rise_to [get_ports out2]

############################################################
# False path with -through
############################################################

set_false_path -from [get_ports in1] -through [get_pins buf1/Z] -to [get_ports out1]

set_false_path -from [get_ports in2] -through [get_pins inv1/ZN] -through [get_pins and1/ZN] -to [get_ports out1]

set_false_path -from [get_ports in1] -through [get_pins or1/ZN] -to [get_ports out2]

# Write to cover through paths
set sdc_file2 [make_result_file sdc_exception_adv2.sdc]
write_sdc -no_timestamp $sdc_file2

# Unset through paths
unset_path_exceptions -from [get_ports in1] -through [get_pins buf1/Z] -to [get_ports out1]
unset_path_exceptions -from [get_ports in2] -through [get_pins inv1/ZN] -through [get_pins and1/ZN] -to [get_ports out1]
unset_path_exceptions -from [get_ports in1] -through [get_pins or1/ZN] -to [get_ports out2]

############################################################
# Multicycle path with rise/fall combinations
############################################################

# Setup multicycle with rise_from
set_multicycle_path -setup 2 -rise_from [get_ports in1] -to [get_ports out1]

# Hold multicycle with fall_to
set_multicycle_path -hold 1 -from [get_ports in1] -fall_to [get_ports out1]

# Multicycle between clock domains
set_multicycle_path -setup 3 -from [get_clocks clk1] -to [get_clocks clk2]

set_multicycle_path -hold 2 -from [get_clocks clk1] -to [get_clocks clk2]

# Multicycle with -through
set_multicycle_path -setup 4 -from [get_ports in2] -through [get_pins and1/ZN] -to [get_ports out1]

# Write to cover multicycle writing
set sdc_file3 [make_result_file sdc_exception_adv3.sdc]
write_sdc -no_timestamp $sdc_file3

# Report
report_checks -from [get_ports in1] -to [get_ports out1]

############################################################
# Unset multicycles and add max/min delay
############################################################

unset_path_exceptions -setup -rise_from [get_ports in1] -to [get_ports out1]
unset_path_exceptions -hold -from [get_ports in1] -fall_to [get_ports out1]
unset_path_exceptions -setup -from [get_clocks clk1] -to [get_clocks clk2]
unset_path_exceptions -hold -from [get_clocks clk1] -to [get_clocks clk2]
unset_path_exceptions -setup -from [get_ports in2] -through [get_pins and1/ZN] -to [get_ports out1]

############################################################
# Max/min delay with various options
############################################################

set_max_delay -from [get_ports in1] -to [get_ports out1] 8.0
set_min_delay -from [get_ports in1] -to [get_ports out1] 1.0

set_max_delay -from [get_ports in2] -through [get_pins and1/ZN] -to [get_ports out1] 6.0

set_max_delay -rise_from [get_ports in3] -fall_to [get_ports out2] 7.0

# Write to cover max/min delay writing
set sdc_file4 [make_result_file sdc_exception_adv4.sdc]
write_sdc -no_timestamp $sdc_file4

# Write compatible mode to cover alternative writer paths
set sdc_file5 [make_result_file sdc_exception_adv5.sdc]
write_sdc -no_timestamp -compatible $sdc_file5

# Write with digits for coverage
set sdc_file6 [make_result_file sdc_exception_adv6.sdc]
write_sdc -no_timestamp -digits 6 $sdc_file6

############################################################
# Group paths (exercises group_path writing)
############################################################

group_path -name reg2reg -from [get_clocks clk1] -to [get_clocks clk1]
group_path -name in2out -from [get_ports {in1 in2 in3}] -to [get_ports {out1 out2}]
group_path -name clk_cross -from [get_clocks clk1] -to [get_clocks clk2]

report_checks -path_group reg2reg

report_checks -path_group in2out

# Write with group paths
set sdc_file7 [make_result_file sdc_exception_adv7.sdc]
write_sdc -no_timestamp $sdc_file7

############################################################
# Read back SDC to verify
############################################################

read_sdc $sdc_file4

report_checks
