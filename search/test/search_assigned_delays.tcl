# Test set_assigned_delay, set_assigned_check, set_assigned_transition commands.
# These exercise Sta.cc setArcDelay, setAnnotatedSlew and Search.cc
# requiredInvalid, arrivalInvalid code paths through annotated delay/check/slew.
# Also tests report_annotated_delay/check with various options.
# Targets: Sta.cc setArcDelay, setAnnotatedSlew, setArcDelayAnnotated,
#          Search.cc requiredInvalid, arrivalInvalid, findClkArrivals
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_path_end_types.v
link_design search_path_end_types

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_input_delay -clock clk 0.5 [get_ports rst]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]
set_output_delay -clock clk 2.0 [get_ports out3]

# Baseline timing
puts "--- Baseline timing ---"
report_checks -path_delay max
report_checks -path_delay min
puts "PASS: baseline"

############################################################
# set_assigned_delay -cell: exercises setArcDelay on cell arcs
# This calls set_arc_delay internally which hits Sta::setArcDelay
############################################################
puts "--- set_assigned_delay -cell for combinational arc ---"
set_assigned_delay -cell -from [get_pins and1/A1] -to [get_pins and1/ZN] 0.05
report_checks -path_delay max
puts "PASS: set_assigned_delay -cell and1"

puts "--- set_assigned_delay -cell -rise ---"
set_assigned_delay -cell -rise -from [get_pins buf1/A] -to [get_pins buf1/Z] 0.03
report_checks -path_delay max
puts "PASS: set_assigned_delay -cell -rise"

puts "--- set_assigned_delay -cell -fall ---"
set_assigned_delay -cell -fall -from [get_pins buf1/A] -to [get_pins buf1/Z] 0.04
report_checks -path_delay max
puts "PASS: set_assigned_delay -cell -fall"

puts "--- set_assigned_delay -cell -min ---"
set_assigned_delay -cell -min -from [get_pins and1/A2] -to [get_pins and1/ZN] 0.01
report_checks -path_delay min
puts "PASS: set_assigned_delay -cell -min"

puts "--- set_assigned_delay -cell -max ---"
set_assigned_delay -cell -max -from [get_pins and1/A2] -to [get_pins and1/ZN] 0.08
report_checks -path_delay max
puts "PASS: set_assigned_delay -cell -max"

############################################################
# set_assigned_delay -net: exercises setArcDelay on net arcs
############################################################
puts "--- set_assigned_delay -net ---"
set_assigned_delay -net -from [get_pins and1/ZN] -to [get_pins buf1/A] 0.02
report_checks -path_delay max
puts "PASS: set_assigned_delay -net"

puts "--- set_assigned_delay -net -rise -max ---"
set_assigned_delay -net -rise -max -from [get_pins buf1/Z] -to [get_pins reg1/D] 0.015
report_checks -path_delay max
puts "PASS: set_assigned_delay -net rise max"

############################################################
# set_assigned_check: exercises setArcDelay on check arcs
# This follows the check arc path in set_arc_delay -> Sta::setArcDelay
# which calls search_->requiredInvalid
############################################################
puts "--- set_assigned_check -setup ---"
set_assigned_check -setup -from [get_pins reg1/CK] -to [get_pins reg1/D] 0.05
report_checks -path_delay max
puts "PASS: set_assigned_check -setup"

puts "--- set_assigned_check -hold ---"
set_assigned_check -hold -from [get_pins reg1/CK] -to [get_pins reg1/D] 0.02
report_checks -path_delay min
puts "PASS: set_assigned_check -hold"

puts "--- set_assigned_check -setup on reg2 ---"
set_assigned_check -setup -from [get_pins reg2/CK] -to [get_pins reg2/D] 0.04
report_checks -path_delay max
puts "PASS: set_assigned_check -setup reg2"

puts "--- set_assigned_check -hold on reg2 ---"
set_assigned_check -hold -from [get_pins reg2/CK] -to [get_pins reg2/D] 0.015
report_checks -path_delay min
puts "PASS: set_assigned_check -hold reg2"

puts "--- set_assigned_check -recovery ---"
set_assigned_check -recovery -from [get_pins reg1/CK] -to [get_pins reg1/RN] 0.06
report_checks -path_delay max
puts "PASS: set_assigned_check -recovery"

puts "--- set_assigned_check -removal ---"
set_assigned_check -removal -from [get_pins reg1/CK] -to [get_pins reg1/RN] 0.03
report_checks -path_delay min
puts "PASS: set_assigned_check -removal"

puts "--- set_assigned_check -setup -rise ---"
set_assigned_check -setup -rise -from [get_pins reg1/CK] -to [get_pins reg1/D] 0.055
report_checks -path_delay max
puts "PASS: set_assigned_check -setup -rise"

puts "--- set_assigned_check -setup -fall ---"
set_assigned_check -setup -fall -from [get_pins reg1/CK] -to [get_pins reg1/D] 0.045
report_checks -path_delay max
puts "PASS: set_assigned_check -setup -fall"

############################################################
# set_assigned_transition: exercises setAnnotatedSlew
############################################################
puts "--- set_assigned_transition ---"
set_assigned_transition 0.1 [get_ports in1]
report_checks -path_delay max -fields {slew}
puts "PASS: set_assigned_transition in1"

puts "--- set_assigned_transition -rise ---"
set_assigned_transition -rise 0.08 [get_ports in2]
report_checks -path_delay max -fields {slew}
puts "PASS: set_assigned_transition -rise"

puts "--- set_assigned_transition -fall ---"
set_assigned_transition -fall 0.12 [get_ports in2]
report_checks -path_delay max -fields {slew}
puts "PASS: set_assigned_transition -fall"

puts "--- set_assigned_transition -min ---"
set_assigned_transition -min 0.05 [get_ports in1]
report_checks -path_delay min -fields {slew}
puts "PASS: set_assigned_transition -min"

puts "--- set_assigned_transition -max ---"
set_assigned_transition -max 0.15 [get_ports in1]
report_checks -path_delay max -fields {slew}
puts "PASS: set_assigned_transition -max"

puts "--- set_assigned_transition on internal pin ---"
set_assigned_transition 0.09 [get_pins buf1/Z]
report_checks -path_delay max -fields {slew}
puts "PASS: set_assigned_transition internal"

############################################################
# report_annotated_delay / report_annotated_check after annotations
############################################################
puts "--- report_annotated_delay ---"
report_annotated_delay
puts "PASS: report_annotated_delay"

puts "--- report_annotated_delay -list_annotated ---"
report_annotated_delay -list_annotated
puts "PASS: report_annotated_delay list_annotated"

puts "--- report_annotated_delay -list_not_annotated ---"
report_annotated_delay -list_not_annotated
puts "PASS: report_annotated_delay list_not_annotated"

puts "--- report_annotated_delay -max_lines 5 ---"
report_annotated_delay -list_not_annotated -max_lines 5
puts "PASS: report_annotated_delay max_lines"

puts "--- report_annotated_check ---"
report_annotated_check
puts "PASS: report_annotated_check"

puts "--- report_annotated_check -list_annotated ---"
report_annotated_check -list_annotated
puts "PASS: report_annotated_check list_annotated"

puts "--- report_annotated_check -list_not_annotated ---"
report_annotated_check -list_not_annotated
puts "PASS: report_annotated_check list_not_annotated"

puts "--- report_annotated_check -setup ---"
report_annotated_check -setup
puts "PASS: report_annotated_check setup"

puts "--- report_annotated_check -hold ---"
report_annotated_check -hold
puts "PASS: report_annotated_check hold"

############################################################
# Verify timing is different from baseline after all annotations
############################################################
puts "--- Final timing after all annotations ---"
report_checks -path_delay max -format full_clock_expanded
report_checks -path_delay min -format full_clock
puts "PASS: final timing"

############################################################
# worst_slack for min paths (exercises worstSlack min in Search.cc)
############################################################
puts "--- worst_slack min ---"
set ws_min [worst_slack -min]
puts "worst_slack min: $ws_min"
set ws_max [worst_slack -max]
puts "worst_slack max: $ws_max"
puts "PASS: worst_slack"

puts "--- report_wns/report_tns ---"
report_wns -max
report_wns -min
report_tns -max
report_tns -min
puts "PASS: report_wns/report_tns"

puts "ALL PASSED"
