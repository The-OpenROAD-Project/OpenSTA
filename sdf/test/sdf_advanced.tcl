# Test SDF advanced features: timing checks, write options, multiple reads,
# gzip write, divider options.
# Targets uncovered SdfReader.cc paths (timing checks, interconnect, cond_use),
# SdfWriter.cc paths (gzip, divider, digits, no_timestamp, no_version,
# include_typ), and SdfParse.yy grammar paths.

source ../../test/helpers.tcl
set test_name sdf_advanced

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdf_test2.v
link_design sdf_test2

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports d]
set_input_delay -clock clk 0 [get_ports en]
set_output_delay -clock clk 0 [get_ports q]

#---------------------------------------------------------------
# Read SDF with timing checks, interconnects, and multi-value triples
#---------------------------------------------------------------
puts "--- read_sdf test2 (with timing checks/interconnects) ---"
read_sdf sdf_test2.sdf
puts "PASS: read_sdf with timing checks"

#---------------------------------------------------------------
# Report annotated delay: exercise all combinations
#---------------------------------------------------------------
puts "--- report_annotated_delay -cell ---"
report_annotated_delay -cell
puts "PASS: annotated delay -cell"

puts "--- report_annotated_delay -net ---"
report_annotated_delay -net
puts "PASS: annotated delay -net"

puts "--- report_annotated_delay -from_in_ports ---"
report_annotated_delay -from_in_ports
puts "PASS: annotated delay -from_in_ports"

puts "--- report_annotated_delay -to_out_ports ---"
report_annotated_delay -to_out_ports
puts "PASS: annotated delay -to_out_ports"

puts "--- report_annotated_delay -cell -net combined ---"
report_annotated_delay -cell -net
puts "PASS: annotated delay -cell -net"

puts "--- report_annotated_delay -report_annotated ---"
report_annotated_delay -report_annotated
puts "PASS: annotated delay -report_annotated"

puts "--- report_annotated_delay -report_unannotated ---"
report_annotated_delay -report_unannotated
puts "PASS: annotated delay -report_unannotated"

puts "--- report_annotated_delay -constant_arcs ---"
report_annotated_delay -constant_arcs
puts "PASS: annotated delay -constant_arcs"

puts "--- report_annotated_delay -max_lines 2 ---"
report_annotated_delay -max_lines 2
puts "PASS: annotated delay -max_lines 2"

#---------------------------------------------------------------
# Report annotated check: exercise all check types
#---------------------------------------------------------------
puts "--- report_annotated_check -setup ---"
report_annotated_check -setup
puts "PASS: annotated check -setup"

puts "--- report_annotated_check -hold ---"
report_annotated_check -hold
puts "PASS: annotated check -hold"

puts "--- report_annotated_check -recovery ---"
report_annotated_check -recovery
puts "PASS: annotated check -recovery"

puts "--- report_annotated_check -removal ---"
report_annotated_check -removal
puts "PASS: annotated check -removal"

puts "--- report_annotated_check -width ---"
report_annotated_check -width
puts "PASS: annotated check -width"

puts "--- report_annotated_check -period ---"
report_annotated_check -period
puts "PASS: annotated check -period"

puts "--- report_annotated_check -nochange ---"
report_annotated_check -nochange
puts "PASS: annotated check -nochange"

puts "--- report_annotated_check -max_skew ---"
report_annotated_check -max_skew
puts "PASS: annotated check -max_skew"

puts "--- report_annotated_check -setup -report_annotated ---"
report_annotated_check -setup -report_annotated
puts "PASS: annotated check -setup -report_annotated"

puts "--- report_annotated_check -setup -report_unannotated ---"
report_annotated_check -setup -report_unannotated
puts "PASS: annotated check -setup -report_unannotated"

puts "--- report_annotated_check -setup -hold combined ---"
report_annotated_check -setup -hold
puts "PASS: annotated check -setup -hold"

puts "--- report_annotated_check all types ---"
report_annotated_check
puts "PASS: annotated check all types"

puts "--- report_annotated_check -constant_arcs ---"
report_annotated_check -constant_arcs
puts "PASS: annotated check -constant_arcs"

puts "--- report_annotated_check -max_lines 3 ---"
report_annotated_check -max_lines 3
puts "PASS: annotated check -max_lines 3"

#---------------------------------------------------------------
# Write SDF with various options
#---------------------------------------------------------------
puts "--- write_sdf default ---"
set sdf_out1 [make_result_file "${test_name}_default.sdf"]
write_sdf $sdf_out1
puts "PASS: write_sdf default"

puts "--- write_sdf -divider . ---"
set sdf_out2 [make_result_file "${test_name}_dot.sdf"]
write_sdf -divider . $sdf_out2
puts "PASS: write_sdf -divider ."

puts "--- write_sdf -digits 6 ---"
set sdf_out3 [make_result_file "${test_name}_digits6.sdf"]
write_sdf -digits 6 $sdf_out3
puts "PASS: write_sdf -digits 6"

puts "--- write_sdf -include_typ ---"
set sdf_out4 [make_result_file "${test_name}_typ.sdf"]
write_sdf -include_typ $sdf_out4
puts "PASS: write_sdf -include_typ"

puts "--- write_sdf -no_timestamp ---"
set sdf_out5 [make_result_file "${test_name}_nots.sdf"]
write_sdf -no_timestamp $sdf_out5
puts "PASS: write_sdf -no_timestamp"

puts "--- write_sdf -no_version ---"
set sdf_out6 [make_result_file "${test_name}_noversion.sdf"]
write_sdf -no_version $sdf_out6
puts "PASS: write_sdf -no_version"

puts "--- write_sdf -gzip ---"
set sdf_out7 [make_result_file "${test_name}_gz.sdf.gz"]
write_sdf -gzip $sdf_out7
puts "PASS: write_sdf -gzip"

puts "--- write_sdf combined options ---"
set sdf_out8 [make_result_file "${test_name}_combined.sdf"]
write_sdf -digits 4 -include_typ -no_timestamp -no_version $sdf_out8
puts "PASS: write_sdf combined options"

#---------------------------------------------------------------
# report_checks with SDF annotations (exercises annotation paths)
#---------------------------------------------------------------
puts "--- report_checks (SDF annotated) ---"
report_checks
puts "PASS: report_checks with SDF"

report_checks -path_delay min
puts "PASS: report_checks min path with SDF"

report_checks -format full_clock
puts "PASS: report_checks full_clock with SDF"

puts "ALL PASSED"
