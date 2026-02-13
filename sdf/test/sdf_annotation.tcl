# Test SDF annotation and reporting commands
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdf_test1.v
link_design sdf_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports d]
set_output_delay -clock clk 0 [get_ports q]

puts "--- read_sdf ---"
read_sdf sdf_test1.sdf
puts "PASS: read_sdf completed"

puts "--- report_annotated_delay (all) ---"
report_annotated_delay

puts "--- report_annotated_delay -cell ---"
report_annotated_delay -cell

puts "--- report_annotated_delay -net ---"
report_annotated_delay -net

puts "--- report_annotated_delay -from_in_ports ---"
report_annotated_delay -from_in_ports

puts "--- report_annotated_delay -to_out_ports ---"
report_annotated_delay -to_out_ports

puts "--- report_annotated_delay -cell -report_annotated ---"
report_annotated_delay -cell -report_annotated

puts "--- report_annotated_delay -cell -report_unannotated ---"
report_annotated_delay -cell -report_unannotated

puts "--- report_annotated_delay -constant_arcs ---"
report_annotated_delay -constant_arcs

puts "--- report_annotated_delay -max_lines 5 ---"
report_annotated_delay -max_lines 5

puts "--- report_annotated_check (all) ---"
report_annotated_check

puts "--- report_annotated_check -setup ---"
report_annotated_check -setup

puts "--- report_annotated_check -hold ---"
report_annotated_check -hold

puts "--- report_annotated_check -recovery ---"
report_annotated_check -recovery

puts "--- report_annotated_check -removal ---"
report_annotated_check -removal

puts "--- report_annotated_check -width ---"
report_annotated_check -width

puts "--- report_annotated_check -period ---"
report_annotated_check -period

puts "--- report_annotated_check -setup -report_annotated ---"
report_annotated_check -setup -report_annotated

puts "--- report_annotated_check -setup -report_unannotated ---"
report_annotated_check -setup -report_unannotated

puts "--- report_annotated_check -constant_arcs ---"
report_annotated_check -constant_arcs

puts "--- report_annotated_check -max_lines 5 ---"
report_annotated_check -max_lines 5

puts "--- report_checks (shows annotated delays) ---"
report_checks

puts "--- report_checks -format full_clock ---"
report_checks -format full_clock

puts "--- report_checks -path_delay min ---"
report_checks -path_delay min

puts "ALL PASSED"
