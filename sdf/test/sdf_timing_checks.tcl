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
puts "PASS: read_sdf"

#---------------------------------------------------------------
# Report timing paths with annotations
#---------------------------------------------------------------
puts "--- report_checks with SDF ---"
report_checks
puts "PASS: report_checks"

report_checks -path_delay min
puts "PASS: min path"

report_checks -path_delay max
puts "PASS: max path"

report_checks -format full_clock
puts "PASS: full_clock"

# Specific paths
report_checks -from [get_ports d1] -to [get_ports q1]
puts "PASS: d1->q1"

report_checks -from [get_ports d1] -to [get_ports q2]
puts "PASS: d1->q2"

report_checks -from [get_ports d1] -to [get_ports q3]
puts "PASS: d1->q3"

report_checks -from [get_ports d2] -to [get_ports q1]
puts "PASS: d2->q1"

report_checks -from [get_ports d2] -to [get_ports q2]
puts "PASS: d2->q2"

report_checks -from [get_ports en] -to [get_ports q2]
puts "PASS: en->q2"

report_checks -from [get_ports en] -to [get_ports q3]
puts "PASS: en->q3"

#---------------------------------------------------------------
# Report annotated delays: exercises all delay annotation filters
#---------------------------------------------------------------
puts "--- report_annotated_delay ---"

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

#---------------------------------------------------------------
# Report annotated checks: exercises all check type filters
#---------------------------------------------------------------
puts "--- report_annotated_check ---"

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

report_annotated_check -setup -hold
puts "PASS: check -setup -hold"

report_annotated_check -setup -hold -recovery -removal
puts "PASS: check setup/hold/recovery/removal"

report_annotated_check -width -period
puts "PASS: check width/period"

report_annotated_check
puts "PASS: check all"

report_annotated_check -report_annotated
puts "PASS: check -report_annotated"

report_annotated_check -report_unannotated
puts "PASS: check -report_unannotated"

report_annotated_check -constant_arcs
puts "PASS: check -constant_arcs"

report_annotated_check -max_lines 2
puts "PASS: check -max_lines 2"

#---------------------------------------------------------------
# Write SDF with various options to exercise SdfWriter paths
#---------------------------------------------------------------
puts "--- write_sdf ---"

set sdf_out1 [make_result_file "${test_name}_default.sdf"]
write_sdf $sdf_out1
puts "PASS: write_sdf default"

set sdf_out2 [make_result_file "${test_name}_d6.sdf"]
write_sdf -digits 6 $sdf_out2
puts "PASS: write_sdf -digits 6"

set sdf_out3 [make_result_file "${test_name}_typ.sdf"]
write_sdf -include_typ $sdf_out3
puts "PASS: write_sdf -include_typ"

set sdf_out4 [make_result_file "${test_name}_dot.sdf"]
write_sdf -divider . $sdf_out4
puts "PASS: write_sdf -divider ."

set sdf_out5 [make_result_file "${test_name}_nots.sdf"]
write_sdf -no_timestamp $sdf_out5
puts "PASS: write_sdf -no_timestamp"

set sdf_out6 [make_result_file "${test_name}_nover.sdf"]
write_sdf -no_version $sdf_out6
puts "PASS: write_sdf -no_version"

set sdf_out7 [make_result_file "${test_name}_combined.sdf"]
write_sdf -digits 4 -include_typ -no_timestamp -no_version $sdf_out7
puts "PASS: write_sdf combined"

#---------------------------------------------------------------
# Re-read SDF to exercise repeated annotation
#---------------------------------------------------------------
puts "--- re-read SDF ---"
read_sdf sdf_test4.sdf
report_checks
puts "PASS: re-read SDF"

puts "ALL PASSED"
