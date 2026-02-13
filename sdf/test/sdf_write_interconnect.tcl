# Test SDF write with interconnect delays from SPEF, various write options,
# and large design coverage.
# Targets: SdfWriter.cc (writeInterconnects, writeInstInterconnects,
#   writeInterconnectFromPin, writeArcDelays, writeSdfTriple,
#   writeSdfDelay, sdfPortName, sdfPathName, sdfName, sdfEdge,
#   writeWidthCheck, writePeriodCheck, writeTimingChecks,
#   writeCheck variations, writeIopaths)
#   SdfReader.cc (re-read from written SDF)
#   ReportAnnotation.cc (annotation with interconnect data)

source ../../test/helpers.tcl
set test_name sdf_write_interconnect

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog ../../examples/example1.v
link_design top

create_clock -name clk -period 500 {clk1 clk2 clk3}
set_input_delay -clock clk 1 {in1 in2}
set_output_delay -clock clk 1 [get_ports out]
set_input_transition 10 {in1 in2 clk1 clk2 clk3}
set_propagated_clock {clk1 clk2 clk3}

#---------------------------------------------------------------
# Read SPEF to get interconnect delays
#---------------------------------------------------------------
puts "--- read SPEF for interconnect ---"
read_spef ../../examples/example1.dspef
puts "PASS: read SPEF (DSPEF)"

report_checks
puts "PASS: report_checks with SPEF"

#---------------------------------------------------------------
# Write SDF with interconnect from SPEF
#---------------------------------------------------------------
puts "--- write_sdf with interconnect ---"
set sdf_out1 [make_result_file "${test_name}_default.sdf"]
write_sdf $sdf_out1
if { [file exists $sdf_out1] && [file size $sdf_out1] > 0 } {
  puts "PASS: write_sdf default size=[file size $sdf_out1]"
}

#---------------------------------------------------------------
# Write SDF with -include_typ (triple values)
#---------------------------------------------------------------
puts "--- write_sdf -include_typ ---"
set sdf_out2 [make_result_file "${test_name}_typ.sdf"]
write_sdf -include_typ $sdf_out2
if { [file exists $sdf_out2] && [file size $sdf_out2] > 0 } {
  puts "PASS: write_sdf -include_typ size=[file size $sdf_out2]"
}

#---------------------------------------------------------------
# Write SDF with -divider .
#---------------------------------------------------------------
puts "--- write_sdf -divider . ---"
set sdf_out3 [make_result_file "${test_name}_dot.sdf"]
write_sdf -divider . $sdf_out3
if { [file exists $sdf_out3] && [file size $sdf_out3] > 0 } {
  puts "PASS: write_sdf -divider . size=[file size $sdf_out3]"
}

#---------------------------------------------------------------
# Write SDF with various digit counts
#---------------------------------------------------------------
puts "--- write_sdf -digits ---"
foreach digits {2 4 6 8} {
  set sdf_d [make_result_file "${test_name}_d${digits}.sdf"]
  write_sdf -digits $digits $sdf_d
  if { [file exists $sdf_d] && [file size $sdf_d] > 0 } {
    puts "PASS: write_sdf -digits $digits size=[file size $sdf_d]"
  }
}

#---------------------------------------------------------------
# Write SDF with -no_timestamp and -no_version
#---------------------------------------------------------------
puts "--- write_sdf -no_timestamp -no_version ---"
set sdf_out4 [make_result_file "${test_name}_clean.sdf"]
write_sdf -no_timestamp -no_version $sdf_out4
if { [file exists $sdf_out4] && [file size $sdf_out4] > 0 } {
  puts "PASS: write_sdf clean size=[file size $sdf_out4]"
}

#---------------------------------------------------------------
# Write SDF gzip
#---------------------------------------------------------------
puts "--- write_sdf -gzip ---"
set sdf_out5 [make_result_file "${test_name}_gz.sdf.gz"]
write_sdf -gzip $sdf_out5
if { [file exists $sdf_out5] && [file size $sdf_out5] > 0 } {
  puts "PASS: write_sdf -gzip created non-empty file"
}

#---------------------------------------------------------------
# Write SDF with all options combined
#---------------------------------------------------------------
puts "--- write_sdf all options ---"
set sdf_out6 [make_result_file "${test_name}_all.sdf"]
write_sdf -digits 4 -include_typ -no_timestamp -no_version -divider . $sdf_out6
if { [file exists $sdf_out6] && [file size $sdf_out6] > 0 } {
  puts "PASS: write_sdf all options size=[file size $sdf_out6]"
}

#---------------------------------------------------------------
# Read SDF back and annotate (roundtrip test)
#---------------------------------------------------------------
puts "--- read back SDF ---"
read_sdf $sdf_out4
puts "PASS: read back written SDF"

report_checks
puts "PASS: report_checks after SDF roundtrip"

#---------------------------------------------------------------
# Report annotated delay with interconnect
#---------------------------------------------------------------
puts "--- annotated delay with interconnect ---"
report_annotated_delay -cell
puts "PASS: annotated delay -cell"

report_annotated_delay -net
puts "PASS: annotated delay -net"

report_annotated_delay -cell -net
puts "PASS: annotated delay -cell -net"

report_annotated_delay -from_in_ports -to_out_ports
puts "PASS: annotated delay from/to ports"

report_annotated_delay -report_annotated
puts "PASS: annotated delay -report_annotated"

report_annotated_delay -report_unannotated
puts "PASS: annotated delay -report_unannotated"

report_annotated_delay -constant_arcs
puts "PASS: annotated delay -constant_arcs"

report_annotated_delay -max_lines 5
puts "PASS: annotated delay -max_lines 5"

#---------------------------------------------------------------
# Report annotated check
#---------------------------------------------------------------
puts "--- annotated check with interconnect ---"
report_annotated_check
puts "PASS: annotated check all"

report_annotated_check -setup
puts "PASS: annotated check -setup"

report_annotated_check -hold
puts "PASS: annotated check -hold"

report_annotated_check -setup -hold
puts "PASS: annotated check -setup -hold"

report_annotated_check -report_annotated
puts "PASS: annotated check -report_annotated"

report_annotated_check -report_unannotated
puts "PASS: annotated check -report_unannotated"

report_annotated_check -max_lines 3
puts "PASS: annotated check -max_lines 3"

#---------------------------------------------------------------
# Read original example SDF to verify reading with different format
#---------------------------------------------------------------
puts "--- read original example SDF ---"
read_sdf ../../examples/example1.sdf
puts "PASS: read example1.sdf"

report_checks
puts "PASS: report_checks with example1.sdf"

report_annotated_delay -cell -net
puts "PASS: annotated delay after example1.sdf"

report_annotated_check -setup -hold
puts "PASS: annotated check after example1.sdf"

#---------------------------------------------------------------
# Write SDF after SDF annotation (exercises annotated delay write)
#---------------------------------------------------------------
puts "--- write SDF after SDF annotation ---"
set sdf_out7 [make_result_file "${test_name}_annotated.sdf"]
write_sdf -no_timestamp -no_version $sdf_out7
if { [file exists $sdf_out7] && [file size $sdf_out7] > 0 } {
  puts "PASS: write_sdf after annotation size=[file size $sdf_out7]"
}

puts "ALL PASSED"
