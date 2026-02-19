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

#---------------------------------------------------------------
# Report annotated delay: all combinations
#---------------------------------------------------------------
puts "--- report_annotated_delay combinations ---"
report_annotated_delay

report_annotated_delay -cell

report_annotated_delay -net

report_annotated_delay -from_in_ports

report_annotated_delay -to_out_ports

report_annotated_delay -cell -net

report_annotated_delay -report_annotated

report_annotated_delay -report_unannotated

report_annotated_delay -constant_arcs

report_annotated_delay -max_lines 3

report_annotated_delay -max_lines 1

#---------------------------------------------------------------
# Report annotated check: all check types
#---------------------------------------------------------------
puts "--- report_annotated_check combinations ---"
report_annotated_check

report_annotated_check -setup

report_annotated_check -hold

report_annotated_check -recovery

report_annotated_check -removal

report_annotated_check -width

report_annotated_check -period

report_annotated_check -nochange

report_annotated_check -max_skew

report_annotated_check -setup -hold

report_annotated_check -setup -hold -recovery -removal

report_annotated_check -report_annotated

report_annotated_check -report_unannotated

report_annotated_check -constant_arcs

report_annotated_check -max_lines 2

#---------------------------------------------------------------
# Write SDF with various option combinations
#---------------------------------------------------------------
puts "--- write_sdf various options ---"

# Default write
set sdf_out1 [make_result_file "${test_name}_default.sdf"]
write_sdf $sdf_out1

# With divider
set sdf_out2 [make_result_file "${test_name}_dot.sdf"]
write_sdf -divider . $sdf_out2

# With digits
set sdf_out3 [make_result_file "${test_name}_d4.sdf"]
write_sdf -digits 4 $sdf_out3

set sdf_out3b [make_result_file "${test_name}_d8.sdf"]
write_sdf -digits 8 $sdf_out3b

# With include_typ
set sdf_out4 [make_result_file "${test_name}_typ.sdf"]
write_sdf -include_typ $sdf_out4

# With no_timestamp
set sdf_out5 [make_result_file "${test_name}_nots.sdf"]
write_sdf -no_timestamp $sdf_out5

# With no_version
set sdf_out6 [make_result_file "${test_name}_nover.sdf"]
write_sdf -no_version $sdf_out6

# Gzip write
set sdf_out7 [make_result_file "${test_name}_gz.sdf.gz"]
write_sdf -gzip $sdf_out7

# All options combined
set sdf_out8 [make_result_file "${test_name}_all.sdf"]
write_sdf -digits 6 -include_typ -no_timestamp -no_version $sdf_out8

# Divider + digits + include_typ
set sdf_out9 [make_result_file "${test_name}_combo.sdf"]
write_sdf -divider . -digits 3 -include_typ $sdf_out9

#---------------------------------------------------------------
# report_checks with SDF annotations (exercises delay comparison)
#---------------------------------------------------------------
puts "--- report_checks with SDF annotations ---"
report_checks

report_checks -path_delay min

report_checks -path_delay max

report_checks -format full_clock

report_checks -fields {slew cap input_pins}

# Different paths through the annotated design
report_checks -from [get_ports d] -to [get_ports q]

report_checks -from [get_ports en] -to [get_ports q]
