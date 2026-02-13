# Test SDF reading with edge specifiers, PORT delays, and write options.
# Targets: SdfReader.cc (edge specifiers, PORT delay, RECOVERY/REMOVAL/PERIOD)
#   SdfWriter.cc (write with various option combinations)
#   ReportAnnotation.cc (annotation reporting with multiple check types)
#   SdfParse.yy (edge specifier grammar paths, PORT DELAY)

source ../../test/helpers.tcl
set test_name sdf_edge_write

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdf_test3.v
link_design sdf_test3

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports d]
set_input_delay -clock clk 0 [get_ports en]
set_output_delay -clock clk 0 [get_ports q]
set_output_delay -clock clk 0 [get_ports q_inv]

#---------------------------------------------------------------
# Read SDF with edge specifiers and timing checks
#---------------------------------------------------------------
puts "--- read_sdf test3 (edge specifiers, RECOVERY/REMOVAL/PERIOD) ---"
read_sdf sdf_test3.sdf
puts "PASS: read_sdf with edge specifiers"

#---------------------------------------------------------------
# Report annotated delay: all combinations
#---------------------------------------------------------------
puts "--- report_annotated_delay combinations ---"
report_annotated_delay
puts "PASS: annotated delay all"

report_annotated_delay -cell
puts "PASS: annotated delay -cell"

report_annotated_delay -net
puts "PASS: annotated delay -net"

report_annotated_delay -from_in_ports
puts "PASS: annotated delay -from_in_ports"

report_annotated_delay -to_out_ports
puts "PASS: annotated delay -to_out_ports"

report_annotated_delay -cell -net
puts "PASS: annotated delay -cell -net"

report_annotated_delay -report_annotated
puts "PASS: annotated delay -report_annotated"

report_annotated_delay -report_unannotated
puts "PASS: annotated delay -report_unannotated"

report_annotated_delay -constant_arcs
puts "PASS: annotated delay -constant_arcs"

report_annotated_delay -max_lines 3
puts "PASS: annotated delay -max_lines 3"

report_annotated_delay -max_lines 1
puts "PASS: annotated delay -max_lines 1"

#---------------------------------------------------------------
# Report annotated check: all check types
#---------------------------------------------------------------
puts "--- report_annotated_check combinations ---"
report_annotated_check
puts "PASS: annotated check all"

report_annotated_check -setup
puts "PASS: annotated check -setup"

report_annotated_check -hold
puts "PASS: annotated check -hold"

report_annotated_check -recovery
puts "PASS: annotated check -recovery"

report_annotated_check -removal
puts "PASS: annotated check -removal"

report_annotated_check -width
puts "PASS: annotated check -width"

report_annotated_check -period
puts "PASS: annotated check -period"

report_annotated_check -nochange
puts "PASS: annotated check -nochange"

report_annotated_check -max_skew
puts "PASS: annotated check -max_skew"

report_annotated_check -setup -hold
puts "PASS: annotated check -setup -hold"

report_annotated_check -setup -hold -recovery -removal
puts "PASS: annotated check -setup -hold -recovery -removal"

report_annotated_check -report_annotated
puts "PASS: annotated check -report_annotated"

report_annotated_check -report_unannotated
puts "PASS: annotated check -report_unannotated"

report_annotated_check -constant_arcs
puts "PASS: annotated check -constant_arcs"

report_annotated_check -max_lines 2
puts "PASS: annotated check -max_lines 2"

#---------------------------------------------------------------
# Write SDF with various option combinations
#---------------------------------------------------------------
puts "--- write_sdf various options ---"

# Default write
set sdf_out1 [make_result_file "${test_name}_default.sdf"]
write_sdf $sdf_out1
puts "PASS: write_sdf default"

# With divider
set sdf_out2 [make_result_file "${test_name}_dot.sdf"]
write_sdf -divider . $sdf_out2
puts "PASS: write_sdf -divider ."

# With digits
set sdf_out3 [make_result_file "${test_name}_d4.sdf"]
write_sdf -digits 4 $sdf_out3
puts "PASS: write_sdf -digits 4"

set sdf_out3b [make_result_file "${test_name}_d8.sdf"]
write_sdf -digits 8 $sdf_out3b
puts "PASS: write_sdf -digits 8"

# With include_typ
set sdf_out4 [make_result_file "${test_name}_typ.sdf"]
write_sdf -include_typ $sdf_out4
puts "PASS: write_sdf -include_typ"

# With no_timestamp
set sdf_out5 [make_result_file "${test_name}_nots.sdf"]
write_sdf -no_timestamp $sdf_out5
puts "PASS: write_sdf -no_timestamp"

# With no_version
set sdf_out6 [make_result_file "${test_name}_nover.sdf"]
write_sdf -no_version $sdf_out6
puts "PASS: write_sdf -no_version"

# Gzip write
set sdf_out7 [make_result_file "${test_name}_gz.sdf.gz"]
write_sdf -gzip $sdf_out7
puts "PASS: write_sdf -gzip"

# All options combined
set sdf_out8 [make_result_file "${test_name}_all.sdf"]
write_sdf -digits 6 -include_typ -no_timestamp -no_version $sdf_out8
puts "PASS: write_sdf all options combined"

# Divider + digits + include_typ
set sdf_out9 [make_result_file "${test_name}_combo.sdf"]
write_sdf -divider . -digits 3 -include_typ $sdf_out9
puts "PASS: write_sdf divider+digits+include_typ"

#---------------------------------------------------------------
# report_checks with SDF annotations (exercises delay comparison)
#---------------------------------------------------------------
puts "--- report_checks with SDF annotations ---"
report_checks
puts "PASS: report_checks with SDF"

report_checks -path_delay min
puts "PASS: report_checks min with SDF"

report_checks -path_delay max
puts "PASS: report_checks max with SDF"

report_checks -format full_clock
puts "PASS: report_checks full_clock with SDF"

report_checks -fields {slew cap input_pins}
puts "PASS: report_checks with fields and SDF"

# Different paths through the annotated design
report_checks -from [get_ports d] -to [get_ports q]
puts "PASS: report_checks d->q with SDF"

report_checks -from [get_ports en] -to [get_ports q]
puts "PASS: report_checks en->q with SDF"

puts "ALL PASSED"
