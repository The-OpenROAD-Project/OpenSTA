# Test SDF reading with timing check annotations (SETUP, HOLD, RECOVERY,
# REMOVAL, WIDTH, PERIOD), interconnect delays, port delays, and
# SDF write with various options.
# Targets:
#   SdfReader.cc: timingCheck, timingCheck1 (SETUP, HOLD),
#     annotateCheckEdges, timingCheckWidth, timingCheckSetupHold,
#     timingCheckRecRem, timingCheckSetupHold1, timingCheckPeriod,
#     timingCheckNochange (notSupported path),
#     interconnect, port, findWireEdge, setEdgeDelays,
#     setEdgeArcDelays, setEdgeArcDelaysCondUse,
#     setCell, setInstance, cellFinish, iopath,
#     setTimescale, setDivider, findPort, findPin,
#     device, setDevicePinDelays, makePortSpec,
#     makeTriple (all overloads), makeTripleSeq, deleteTripleSeq,
#     unescaped, makePath, makeBusName,
#     setInTimingCheck, setInIncremental
#   SdfWriter.cc: writeTimingChecks, writeCheck, writeEdgeCheck,
#     writeTimingCheckHeader/Trailer, writeWidthCheck, writePeriodCheck

source ../../test/helpers.tcl
set test_name sdf_timing_checks

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdf_test4.v
link_design sdf_test4

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {d1 d2 en}]
set_output_delay -clock clk 0 [get_ports {q1 q2 q3}]
set_input_transition 0.1 [get_ports {d1 d2 en clk}]

#---------------------------------------------------------------
# Read SDF with comprehensive timing checks
#---------------------------------------------------------------
puts "--- read_sdf with timing checks ---"
read_sdf sdf_test4.sdf

#---------------------------------------------------------------
# Report timing paths with annotations
#---------------------------------------------------------------
puts "--- report_checks with SDF ---"
report_checks

report_checks -path_delay min

report_checks -path_delay max

report_checks -format full_clock

# Specific paths
report_checks -from [get_ports d1] -to [get_ports q1]

report_checks -from [get_ports d1] -to [get_ports q2]

report_checks -from [get_ports d1] -to [get_ports q3]

report_checks -from [get_ports d2] -to [get_ports q1]

report_checks -from [get_ports d2] -to [get_ports q2]

report_checks -from [get_ports en] -to [get_ports q2]

report_checks -from [get_ports en] -to [get_ports q3]

#---------------------------------------------------------------
# Report annotated delays: exercises all delay annotation filters
#---------------------------------------------------------------
puts "--- report_annotated_delay ---"

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

#---------------------------------------------------------------
# Report annotated checks: exercises all check type filters
#---------------------------------------------------------------
puts "--- report_annotated_check ---"

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

report_annotated_check -width -period

report_annotated_check

report_annotated_check -report_annotated

report_annotated_check -report_unannotated

report_annotated_check -constant_arcs

report_annotated_check -max_lines 2

#---------------------------------------------------------------
# Write SDF with various options to exercise SdfWriter paths
#---------------------------------------------------------------
puts "--- write_sdf ---"

set sdf_out1 [make_result_file "${test_name}_default.sdf"]
write_sdf $sdf_out1

set sdf_out2 [make_result_file "${test_name}_d6.sdf"]
write_sdf -digits 6 $sdf_out2

set sdf_out3 [make_result_file "${test_name}_typ.sdf"]
write_sdf -include_typ $sdf_out3

set sdf_out4 [make_result_file "${test_name}_dot.sdf"]
write_sdf -divider . $sdf_out4

set sdf_out5 [make_result_file "${test_name}_nots.sdf"]
write_sdf -no_timestamp $sdf_out5

set sdf_out6 [make_result_file "${test_name}_nover.sdf"]
write_sdf -no_version $sdf_out6

set sdf_out7 [make_result_file "${test_name}_combined.sdf"]
write_sdf -digits 4 -include_typ -no_timestamp -no_version $sdf_out7

#---------------------------------------------------------------
# Re-read SDF to exercise repeated annotation
#---------------------------------------------------------------
puts "--- re-read SDF ---"
read_sdf sdf_test4.sdf
report_checks
