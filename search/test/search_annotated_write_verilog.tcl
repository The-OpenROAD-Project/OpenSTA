# Test annotated delays, annotated checks, annotated slews,
# write_verilog, write_sdf with various dividers, report_disabled_edges,
# report_annotated_delay and report_annotated_check with various options.
# Targets: Sta.cc setArcDelayAnnotated, arcDelayAnnotated,
#          setAnnotatedSlew, removeDelaySlewAnnotations,
#          writeSdf with divider/include_typ/digits,
#          Sdf.cc SdfWriter.cc (write_sdf_cmd),
#          Graph.cc report_disabled_edges
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

# Baseline timing
report_checks -path_delay max > /dev/null

############################################################
# report_annotated_delay with various options
############################################################
puts "--- report_annotated_delay ---"
report_annotated_delay
puts "PASS: report_annotated_delay default"

puts "--- report_annotated_delay -list_annotated ---"
report_annotated_delay -list_annotated
puts "PASS: report_annotated_delay list_annotated"

puts "--- report_annotated_delay -list_not_annotated ---"
report_annotated_delay -list_not_annotated
puts "PASS: report_annotated_delay list_not_annotated"

puts "--- report_annotated_delay -list_not_annotated -max_lines 5 ---"
report_annotated_delay -list_not_annotated -max_lines 5
puts "PASS: report_annotated_delay max_lines"

puts "--- report_annotated_delay -constant_arcs ---"
report_annotated_delay -constant_arcs
puts "PASS: report_annotated_delay constant_arcs"

############################################################
# report_annotated_check with various options
############################################################
puts "--- report_annotated_check ---"
report_annotated_check
puts "PASS: report_annotated_check default"

puts "--- report_annotated_check -setup ---"
report_annotated_check -setup
puts "PASS: report_annotated_check setup"

puts "--- report_annotated_check -hold ---"
report_annotated_check -hold
puts "PASS: report_annotated_check hold"

puts "--- report_annotated_check -setup -hold ---"
report_annotated_check -setup -hold
puts "PASS: report_annotated_check setup+hold"

puts "--- report_annotated_check -list_annotated ---"
report_annotated_check -list_annotated
puts "PASS: report_annotated_check list_annotated"

puts "--- report_annotated_check -list_not_annotated ---"
report_annotated_check -list_not_annotated
puts "PASS: report_annotated_check list_not_annotated"

puts "--- report_annotated_check -recovery ---"
report_annotated_check -recovery
puts "PASS: report_annotated_check recovery"

puts "--- report_annotated_check -removal ---"
report_annotated_check -removal
puts "PASS: report_annotated_check removal"

puts "--- report_annotated_check -width ---"
report_annotated_check -width
puts "PASS: report_annotated_check width"

puts "--- report_annotated_check -period ---"
report_annotated_check -period
puts "PASS: report_annotated_check period"

puts "--- report_annotated_check -max_skew ---"
report_annotated_check -max_skew
puts "PASS: report_annotated_check max_skew"

puts "--- report_annotated_check -nochange ---"
report_annotated_check -nochange
puts "PASS: report_annotated_check nochange"

############################################################
# report_disabled_edges
############################################################
puts "--- report_disabled_edges ---"
report_disabled_edges
puts "PASS: report_disabled_edges default"

############################################################
# Disable some timing, check disabled edges
############################################################
puts "--- disable + report_disabled_edges ---"
set_disable_timing [get_cells buf1]
report_disabled_edges
report_checks -path_delay max
unset_disable_timing [get_cells buf1]
puts "PASS: disable + report_disabled_edges"

puts "--- disable lib cell + report_disabled_edges ---"
set_disable_timing [get_lib_cells NangateOpenCellLibrary/BUF_X1] -from A -to Z
report_disabled_edges
report_checks -path_delay max
unset_disable_timing [get_lib_cells NangateOpenCellLibrary/BUF_X1] -from A -to Z
puts "PASS: disable lib cell + report_disabled_edges"

############################################################
# write_sdf with different dividers and options
############################################################
puts "--- write_sdf divider . ---"
set sdf1 [make_result_file "annotated_dot.sdf"]
write_sdf -divider . -no_timestamp -no_version $sdf1
puts "PASS: write_sdf divider ."

puts "--- write_sdf divider / ---"
set sdf2 [make_result_file "annotated_slash.sdf"]
write_sdf -divider / -no_timestamp -no_version $sdf2
puts "PASS: write_sdf divider /"

puts "--- write_sdf include_typ ---"
set sdf3 [make_result_file "annotated_typ.sdf"]
write_sdf -include_typ -no_timestamp -no_version $sdf3
puts "PASS: write_sdf include_typ"

puts "--- write_sdf digits 6 ---"
set sdf4 [make_result_file "annotated_d6.sdf"]
write_sdf -digits 6 -no_timestamp -no_version $sdf4
puts "PASS: write_sdf digits 6"

puts "--- write_sdf digits 1 ---"
set sdf5 [make_result_file "annotated_d1.sdf"]
write_sdf -digits 1 -no_timestamp -no_version $sdf5
puts "PASS: write_sdf digits 1"

############################################################
# write_verilog with various options
############################################################
puts "--- write_verilog ---"
set v1 [make_result_file "annotated_out.v"]
write_verilog $v1
puts "PASS: write_verilog default"

puts "--- write_verilog -include_pwr_gnd ---"
set v2 [make_result_file "annotated_pwr.v"]
write_verilog -include_pwr_gnd $v2
puts "PASS: write_verilog include_pwr_gnd"

puts "--- write_verilog -remove_cells ---"
set v3 [make_result_file "annotated_remove.v"]
write_verilog -remove_cells {} $v3
puts "PASS: write_verilog remove_cells"

############################################################
# read_sdf after write_sdf
############################################################
puts "--- read_sdf ---"
catch {
  read_sdf $sdf1
  report_checks -path_delay max
}
puts "PASS: read_sdf"

puts "--- report_annotated_delay after read_sdf ---"
report_annotated_delay -list_annotated
puts "PASS: report_annotated after read_sdf"

puts "--- report_annotated_check after read_sdf ---"
report_annotated_check -list_annotated -setup -hold
puts "PASS: report_annotated_check after read_sdf"

############################################################
# remove delay/slew annotations
############################################################
puts "--- remove_delay_slew_annotations ---"
catch { sta::remove_delay_slew_annotations }
puts "PASS: remove_delay_slew_annotations"

puts "--- report_annotated_delay after remove ---"
report_annotated_delay
puts "PASS: report_annotated_delay after remove"

puts "ALL PASSED"
