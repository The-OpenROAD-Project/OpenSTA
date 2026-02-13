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
puts "PASS: read_sdf with timing checks"

#---------------------------------------------------------------
# Report timing checks
#---------------------------------------------------------------
puts "--- report_checks setup ---"
report_checks
puts "PASS: report_checks"

report_checks -path_delay min
puts "PASS: report_checks min"

report_checks -format full_clock
puts "PASS: report_checks full_clock"

report_checks -from [get_ports d] -to [get_ports q]
puts "PASS: d->q"

report_checks -from [get_ports d] -to [get_ports q_inv]
puts "PASS: d->q_inv"

report_checks -from [get_ports en] -to [get_ports q]
puts "PASS: en->q"

#---------------------------------------------------------------
# report_annotated_delay with all options
#---------------------------------------------------------------
puts "--- report_annotated_delay ---"
report_annotated_delay -cell
puts "PASS: annotated delay -cell"

report_annotated_delay -net
puts "PASS: annotated delay -net"

report_annotated_delay -cell -net
puts "PASS: annotated delay -cell -net"

report_annotated_delay -from_in_ports
puts "PASS: annotated delay -from_in_ports"

report_annotated_delay -to_out_ports
puts "PASS: annotated delay -to_out_ports"

report_annotated_delay -from_in_ports -to_out_ports
puts "PASS: annotated delay -from_in_ports -to_out_ports"

report_annotated_delay -report_annotated
puts "PASS: annotated delay -report_annotated"

report_annotated_delay -report_unannotated
puts "PASS: annotated delay -report_unannotated"

report_annotated_delay -constant_arcs
puts "PASS: annotated delay -constant_arcs"

report_annotated_delay -max_lines 5
puts "PASS: annotated delay -max_lines 5"

report_annotated_delay -max_lines 1
puts "PASS: annotated delay -max_lines 1"

#---------------------------------------------------------------
# report_annotated_check with all check types
#---------------------------------------------------------------
puts "--- report_annotated_check all types individually ---"
report_annotated_check -setup
puts "PASS: check -setup"

report_annotated_check -hold
puts "PASS: check -hold"

report_annotated_check -recovery
puts "PASS: check -recovery"

report_annotated_check -removal
puts "PASS: check -removal"

report_annotated_check -width
puts "PASS: check -width"

report_annotated_check -period
puts "PASS: check -period"

report_annotated_check -nochange
puts "PASS: check -nochange"

report_annotated_check -max_skew
puts "PASS: check -max_skew"

puts "--- report_annotated_check combined ---"
report_annotated_check -setup -hold
puts "PASS: check -setup -hold"

report_annotated_check -setup -hold -recovery -removal
puts "PASS: check setup/hold/recovery/removal"

report_annotated_check -width -period
puts "PASS: check width/period"

report_annotated_check
puts "PASS: check all (default)"

report_annotated_check -report_annotated
puts "PASS: check -report_annotated"

report_annotated_check -report_unannotated
puts "PASS: check -report_unannotated"

report_annotated_check -constant_arcs
puts "PASS: check -constant_arcs"

report_annotated_check -max_lines 2
puts "PASS: check -max_lines 2"

#---------------------------------------------------------------
# Write SDF with timing checks to exercise writer paths
#---------------------------------------------------------------
puts "--- write_sdf with check annotations ---"
set sdf_out1 [make_result_file "${test_name}_default.sdf"]
write_sdf -no_timestamp -no_version $sdf_out1
puts "PASS: write_sdf default"

set sdf_out2 [make_result_file "${test_name}_d6.sdf"]
write_sdf -no_timestamp -no_version -digits 6 $sdf_out2
puts "PASS: write_sdf digits 6"

set sdf_out3 [make_result_file "${test_name}_typ.sdf"]
write_sdf -no_timestamp -no_version -include_typ $sdf_out3
puts "PASS: write_sdf include_typ"

set sdf_out4 [make_result_file "${test_name}_dot.sdf"]
write_sdf -no_timestamp -no_version -divider . $sdf_out4
puts "PASS: write_sdf divider ."

set sdf_out5 [make_result_file "${test_name}_d2.sdf"]
write_sdf -no_timestamp -no_version -digits 2 $sdf_out5
puts "PASS: write_sdf digits 2"

set sdf_out6 [make_result_file "${test_name}_d8.sdf"]
write_sdf -no_timestamp -no_version -digits 8 $sdf_out6
puts "PASS: write_sdf digits 8"

puts "ALL PASSED"
