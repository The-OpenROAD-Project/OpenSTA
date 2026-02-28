# Test SDF reading with DEVICE delays, edge specifiers (posedge/negedge)
# on timing check data signals, SETUPHOLD, RECREM, NOCHANGE, SKEW,
# INCREMENT annotations, and various triple formats.
# Targets:
#   SdfReader.cc: device(SdfTripleSeq*), device(string*, SdfTripleSeq*),
#     setDevicePinDelays, timingCheck with edge specifiers on data,
#     timingCheckSetupHold, timingCheckRecRem, timingCheckNochange (notSupported),
#     timingCheck for skew role, setInIncremental, incremental annotation paths,
#     setEdgeArcDelays in_incremental_ path, makeTriple, makeTripleSeq
#   SdfWriter.cc: writeTimingChecks with edge specifiers

source ../../test/helpers.tcl
set test_name sdf_device_cond

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdf_test5.v
link_design sdf_test5

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {d1 d2 en}]
set_output_delay -clock clk 0 [get_ports {q1 q2 q3}]
set_input_transition 0.1 [get_ports {d1 d2 en clk}]

#---------------------------------------------------------------
# Test 1: Read SDF with DEVICE and edge timing checks
#---------------------------------------------------------------
puts "--- Test 1: read SDF with DEVICE/edge checks ---"
read_sdf sdf_test5.sdf

#---------------------------------------------------------------
# Test 2: Report timing paths (DEVICE delays affect paths)
#---------------------------------------------------------------
puts "--- Test 2: timing paths ---"
report_checks

report_checks -path_delay min

report_checks -from [get_ports d1] -to [get_ports q1]

report_checks -from [get_ports d2] -to [get_ports q1]

report_checks -from [get_ports d1] -to [get_ports q3]

report_checks -from [get_ports en] -to [get_ports q2]

#---------------------------------------------------------------
# Test 3: Report annotated delays and checks
#---------------------------------------------------------------
puts "--- Test 3: annotated reports ---"

report_annotated_delay

report_annotated_delay -cell

report_annotated_delay -net

report_annotated_delay -from_in_ports

report_annotated_delay -to_out_ports

report_annotated_delay -report_annotated

report_annotated_delay -report_unannotated

report_annotated_check -setup

report_annotated_check -hold

report_annotated_check -recovery

report_annotated_check -removal

report_annotated_check -width

report_annotated_check -period

report_annotated_check -nochange

report_annotated_check -max_skew

report_annotated_check

#---------------------------------------------------------------
# Test 4: Write SDF and verify DEVICE output
#---------------------------------------------------------------
puts "--- Test 4: write SDF ---"

set sdf_out1 [make_result_file "${test_name}_out.sdf"]
write_sdf $sdf_out1

set sdf_out2 [make_result_file "${test_name}_typ.sdf"]
write_sdf -include_typ $sdf_out2

set sdf_out3 [make_result_file "${test_name}_combined.sdf"]
write_sdf -digits 6 -no_timestamp -no_version $sdf_out3

#---------------------------------------------------------------
# Test 5: Read incremental SDF
# Exercises: setInIncremental(true), incremental annotation paths
#---------------------------------------------------------------
puts "--- Test 5: incremental SDF ---"
read_sdf sdf_test5_incr.sdf

report_checks

report_annotated_delay

report_annotated_delay -report_annotated

#---------------------------------------------------------------
# Test 6: Re-read absolute SDF on top of incremental
#---------------------------------------------------------------
puts "--- Test 6: re-read absolute SDF ---"
read_sdf sdf_test5.sdf

report_checks

report_annotated_check -setup -hold

#---------------------------------------------------------------
# Test 7: Detailed path reports
#---------------------------------------------------------------
puts "--- Test 7: detailed reports ---"
report_checks -fields {slew cap input_pins nets fanout}

report_checks -format full_clock

report_checks -digits 6

report_check_types -max_delay -verbose

report_check_types -min_delay -verbose
