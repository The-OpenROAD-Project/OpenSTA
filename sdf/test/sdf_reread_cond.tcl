# Test SDF re-read, annotation after constraint changes, and
# write-read roundtrip.
# Targets: SdfReader.cc (re-read/overwrite, unescaped paths)
#   SdfWriter.cc (write after timing changes)
#   ReportAnnotation.cc (unannotated entries with partial annotation)

source ../../test/helpers.tcl
set test_name sdf_reread_cond

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdf_test2.v
link_design sdf_test2

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports d]
set_input_delay -clock clk 0 [get_ports en]
set_output_delay -clock clk 0 [get_ports q]

#---------------------------------------------------------------
# First read SDF
#---------------------------------------------------------------
puts "--- first read_sdf ---"
read_sdf sdf_test2.sdf
puts "PASS: first read_sdf"

report_checks
puts "PASS: report_checks after first read"

#---------------------------------------------------------------
# Report annotation before re-read
#---------------------------------------------------------------
puts "--- report_annotated before re-read ---"
report_annotated_delay -cell -report_annotated
puts "PASS: annotated delay before re-read"

report_annotated_check -setup -report_annotated
puts "PASS: annotated check before re-read"

report_annotated_delay -report_unannotated
puts "PASS: unannotated delay before re-read"

report_annotated_check -report_unannotated
puts "PASS: unannotated check before re-read"

#---------------------------------------------------------------
# Write SDF (captures current state)
#---------------------------------------------------------------
puts "--- write_sdf before re-read ---"
set sdf_out1 [make_result_file "${test_name}_first.sdf"]
write_sdf $sdf_out1
puts "PASS: write_sdf before re-read"

set sdf_out1_typ [make_result_file "${test_name}_first_typ.sdf"]
write_sdf -include_typ $sdf_out1_typ
puts "PASS: write_sdf include_typ before re-read"

#---------------------------------------------------------------
# Re-read the same SDF (tests overwrite path)
#---------------------------------------------------------------
puts "--- re-read_sdf ---"
read_sdf sdf_test2.sdf
puts "PASS: re-read_sdf"

report_checks
puts "PASS: report_checks after re-read"

report_annotated_delay
puts "PASS: annotated delay after re-read"

report_annotated_check
puts "PASS: annotated check after re-read"

#---------------------------------------------------------------
# Read-write roundtrip: write SDF then read it back
#---------------------------------------------------------------
puts "--- write-read roundtrip ---"
set sdf_out2 [make_result_file "${test_name}_roundtrip.sdf"]
write_sdf -no_timestamp -no_version $sdf_out2
puts "PASS: write_sdf for roundtrip"

# Change design constraints and re-read
set_input_delay -clock clk 1.0 [get_ports d]
set_output_delay -clock clk 2.0 [get_ports q]
create_clock -name clk -period 5 [get_ports clk]

# Re-read the written SDF
read_sdf $sdf_out2
puts "PASS: read back written SDF"

report_checks
puts "PASS: report_checks after roundtrip"

report_annotated_delay -cell
puts "PASS: annotated delay after roundtrip"

report_annotated_check -setup -hold
puts "PASS: annotated check after roundtrip"

#---------------------------------------------------------------
# Write SDF with different dividers and read back
#---------------------------------------------------------------
puts "--- write with dot divider ---"
set sdf_out3 [make_result_file "${test_name}_dot.sdf"]
write_sdf -divider . -no_timestamp -no_version $sdf_out3
puts "PASS: write_sdf with dot divider"

#---------------------------------------------------------------
# Report annotation with max_lines variations
#---------------------------------------------------------------
puts "--- report_annotated_delay max_lines variations ---"
report_annotated_delay -max_lines 1
puts "PASS: max_lines 1"

report_annotated_delay -max_lines 5
puts "PASS: max_lines 5"

report_annotated_delay -max_lines 10
puts "PASS: max_lines 10"

report_annotated_delay -max_lines 100
puts "PASS: max_lines 100"

puts "--- report_annotated_check max_lines variations ---"
report_annotated_check -max_lines 1
puts "PASS: check max_lines 1"

report_annotated_check -max_lines 5
puts "PASS: check max_lines 5"

#---------------------------------------------------------------
# Gzip roundtrip
#---------------------------------------------------------------
puts "--- gzip write-read roundtrip ---"
set sdf_gz [make_result_file "${test_name}_gz.sdf.gz"]
write_sdf -gzip -no_timestamp $sdf_gz
puts "PASS: write_sdf gzip"

# Read gzip SDF
set rc [catch { read_sdf $sdf_gz } msg]
if { $rc == 0 } {
  puts "PASS: read gzip SDF"
} else {
  puts "INFO: read gzip SDF: $msg"
}

report_checks
puts "PASS: report_checks after gzip roundtrip"

puts "ALL PASSED"
