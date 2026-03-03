# Test SDF timing check annotation and reporting
# Targets: SdfWriter.cc: writeTimingChecks, writeCheck, writeEdgeCheck,
#   writeTimingCheckHeader/Trailer, writeWidthCheck, writePeriodCheck,
#   sdfEdge, writeInstances, ensureTimingCheckHeaders
# Targets: ReportAnnotation.cc: reportAnnotatedCheck with all check types,
#   reportCheckCounts, reportCheckCount, findPeriodCount,
#   findCounts with various filter options

source ../../test/helpers.tcl
set test_name sdf_check_annotation

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdf_test3.v
link_design sdf_test3

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {d en}]
set_output_delay -clock clk 0 [get_ports {q q_inv}]
set_input_transition 0.1 [get_ports {d en clk}]

#---------------------------------------------------------------
# Read SDF with timing checks (SETUP, HOLD, RECOVERY, REMOVAL,
# WIDTH, PERIOD)
#---------------------------------------------------------------
puts "--- read_sdf with timing checks ---"
read_sdf sdf_test3.sdf

#---------------------------------------------------------------
# Report timing checks
#---------------------------------------------------------------
puts "--- report_checks setup ---"
report_checks

report_checks -path_delay min

report_checks -format full_clock

report_checks -from [get_ports d] -to [get_ports q]

report_checks -from [get_ports d] -to [get_ports q_inv]

report_checks -from [get_ports en] -to [get_ports q]

#---------------------------------------------------------------
# report_annotated_delay with all options
#---------------------------------------------------------------
puts "--- report_annotated_delay ---"
report_annotated_delay -cell

report_annotated_delay -net

report_annotated_delay -cell -net

report_annotated_delay -from_in_ports

report_annotated_delay -to_out_ports

report_annotated_delay -from_in_ports -to_out_ports

report_annotated_delay -report_annotated

report_annotated_delay -report_unannotated

report_annotated_delay -constant_arcs

report_annotated_delay -max_lines 5

report_annotated_delay -max_lines 1

#---------------------------------------------------------------
# report_annotated_check with all check types
#---------------------------------------------------------------
puts "--- report_annotated_check all types individually ---"
report_annotated_check -setup

report_annotated_check -hold

report_annotated_check -recovery

report_annotated_check -removal

report_annotated_check -width

report_annotated_check -period

report_annotated_check -nochange

report_annotated_check -max_skew

puts "--- report_annotated_check combined ---"
report_annotated_check -setup -hold

report_annotated_check -setup -hold -recovery -removal

report_annotated_check -width -period

report_annotated_check

report_annotated_check -report_annotated

report_annotated_check -report_unannotated

report_annotated_check -constant_arcs

report_annotated_check -max_lines 2

#---------------------------------------------------------------
# Write SDF with timing checks to exercise writer paths
#---------------------------------------------------------------
puts "--- write_sdf with check annotations ---"
set sdf_out1 [make_result_file "${test_name}_default.sdf"]
write_sdf -no_timestamp -no_version $sdf_out1

set sdf_out2 [make_result_file "${test_name}_d6.sdf"]
write_sdf -no_timestamp -no_version -digits 6 $sdf_out2

set sdf_out3 [make_result_file "${test_name}_typ.sdf"]
write_sdf -no_timestamp -no_version -include_typ $sdf_out3

set sdf_out4 [make_result_file "${test_name}_dot.sdf"]
write_sdf -no_timestamp -no_version -divider . $sdf_out4

set sdf_out5 [make_result_file "${test_name}_d2.sdf"]
write_sdf -no_timestamp -no_version -digits 2 $sdf_out5

set sdf_out6 [make_result_file "${test_name}_d8.sdf"]
write_sdf -no_timestamp -no_version -digits 8 $sdf_out6
