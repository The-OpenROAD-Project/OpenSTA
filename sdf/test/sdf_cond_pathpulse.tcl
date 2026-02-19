# Test SDF reading with edge specifiers on timing checks (posedge/negedge
# on data), SETUPHOLD, RECREM, SKEW timing checks, and comprehensive
# annotation + write options.
# Targets:
#   SdfReader.cc: timingCheck with edge specifiers on data signals,
#     timingCheckSetupHold, timingCheckRecRem,
#     timingCheck for SKEW role, annotateCheckEdges with edge data,
#     setEdgeArcDelays for posedge/negedge on clock pins,
#     makePortSpec with edge, makeTriple, makeTripleSeq,
#     iopath, setCell, setInstance, cellFinish,
#     interconnect, port, findWireEdge, setEdgeDelays
#   SdfWriter.cc: writeTimingChecks, writeCheck with edge data

source ../../test/helpers.tcl
set test_name sdf_cond_pathpulse

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdf_cond_pathpulse.v
link_design sdf_cond_pathpulse

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {d1 d2 sel}]
set_output_delay -clock clk 0 [get_ports {q1 q2}]
set_input_transition 0.1 [get_ports {d1 d2 sel clk}]

#---------------------------------------------------------------
# Test 1: Read SDF with edge checks
#---------------------------------------------------------------
puts "--- Test 1: read_sdf ---"
read_sdf sdf_cond_pathpulse.sdf

#---------------------------------------------------------------
# Test 2: Report timing paths
#---------------------------------------------------------------
puts "--- Test 2: timing paths ---"
report_checks

report_checks -path_delay min

report_checks -path_delay max -format full_clock

report_checks -path_delay max -format full_clock_expanded

report_checks -from [get_ports d1] -to [get_ports q1]

report_checks -from [get_ports d2] -to [get_ports q1]

report_checks -from [get_ports sel] -to [get_ports q1]

report_checks -from [get_ports d1] -to [get_ports q2]

report_checks -from [get_ports sel] -to [get_ports q2]

#---------------------------------------------------------------
# Test 3: Report annotated delays
#---------------------------------------------------------------
puts "--- Test 3: annotated reports ---"

report_annotated_delay

report_annotated_delay -cell

report_annotated_delay -net

report_annotated_delay -from_in_ports

report_annotated_delay -to_out_ports

report_annotated_delay -report_annotated

report_annotated_delay -report_unannotated

#---------------------------------------------------------------
# Test 4: Report annotated checks
#---------------------------------------------------------------
puts "--- Test 4: annotated checks ---"

report_annotated_check -setup

report_annotated_check -hold

report_annotated_check -recovery

report_annotated_check -removal

report_annotated_check -width

report_annotated_check -period

report_annotated_check -nochange

report_annotated_check -max_skew

report_annotated_check

report_annotated_check -report_annotated

report_annotated_check -report_unannotated

#---------------------------------------------------------------
# Test 5: Write SDF and verify
#---------------------------------------------------------------
puts "--- Test 5: write SDF ---"

set sdf_out1 [make_result_file "${test_name}_out.sdf"]
write_sdf $sdf_out1

set sdf_out2 [make_result_file "${test_name}_typ.sdf"]
write_sdf -include_typ $sdf_out2

set sdf_out3 [make_result_file "${test_name}_d6.sdf"]
write_sdf -digits 6 -no_timestamp -no_version $sdf_out3

set sdf_out4 [make_result_file "${test_name}_dot.sdf"]
write_sdf -divider . $sdf_out4

#---------------------------------------------------------------
# Test 6: Detailed reports with various fields
#---------------------------------------------------------------
puts "--- Test 6: detailed reports ---"

report_checks -fields {slew cap input_pins nets fanout}

report_checks -digits 6

report_check_types -max_delay -verbose

report_check_types -min_delay -verbose

report_check_types -recovery -verbose

report_check_types -removal -verbose

report_check_types -min_pulse_width -verbose

report_check_types -min_period -verbose

report_check_types -max_skew -verbose

#---------------------------------------------------------------
# Test 7: Read SDF again to exercise re-annotation
#---------------------------------------------------------------
puts "--- Test 7: re-read SDF ---"
read_sdf sdf_cond_pathpulse.sdf
report_checks

#---------------------------------------------------------------
# Test 8: Write SDF with all options combined
#---------------------------------------------------------------
puts "--- Test 8: combined write ---"
set sdf_combined [make_result_file "${test_name}_combined.sdf"]
write_sdf -digits 4 -include_typ -no_timestamp -no_version $sdf_combined
