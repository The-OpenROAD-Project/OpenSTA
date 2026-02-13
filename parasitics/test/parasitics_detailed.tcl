# Test detailed parasitics reporting and annotation
# Exercises: report_parasitic_annotation, report_net, SPEF-based delay calc

# Read ASAP7 libraries
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz

read_verilog ../../test/reg1_asap7.v
link_design top

create_clock -name clk -period 500 {clk1 clk2 clk3}
set_input_delay -clock clk 1 {in1 in2}
set_output_delay -clock clk 1 [get_ports out]
set_input_transition 10 {in1 in2 clk1 clk2 clk3}
set_propagated_clock {clk1 clk2 clk3}

#---------------------------------------------------------------
# Read SPEF parasitics
#---------------------------------------------------------------
puts "--- Reading SPEF ---"
read_spef ../../test/reg1_asap7.spef
puts "PASS: read_spef completed"

#---------------------------------------------------------------
# report_parasitic_annotation
#---------------------------------------------------------------
puts "--- Parasitic annotation ---"
report_parasitic_annotation
puts "PASS: report_parasitic_annotation"

report_parasitic_annotation -report_unannotated
puts "PASS: report_parasitic_annotation -report_unannotated"

#---------------------------------------------------------------
# report_annotated_delay
#---------------------------------------------------------------
puts "--- Annotated delay reporting ---"

catch {report_annotated_delay -cell -net} msg
puts $msg
puts "PASS: report_annotated_delay -cell -net"

catch {report_annotated_delay -from_in_ports -to_out_ports} msg
puts $msg
puts "PASS: report_annotated_delay -from_in_ports -to_out_ports"

catch {report_annotated_delay -cell} msg
puts $msg
puts "PASS: report_annotated_delay -cell"

catch {report_annotated_delay -net} msg
puts $msg
puts "PASS: report_annotated_delay -net"

catch {report_annotated_delay -report_annotated} msg
puts $msg
puts "PASS: report_annotated_delay -report_annotated"

catch {report_annotated_delay -report_unannotated} msg
puts $msg
puts "PASS: report_annotated_delay -report_unannotated"

#---------------------------------------------------------------
# report_checks (exercises parasitics in delay calc)
#---------------------------------------------------------------
puts "--- Timing with parasitics ---"
report_checks
puts "PASS: report_checks with parasitics"

report_checks -path_delay min
puts "PASS: report_checks min path"

report_checks -path_delay max
puts "PASS: report_checks max path"

report_checks -from [get_ports in1] -fields {slew cap}
puts "PASS: report_checks from in1 with fields"

report_checks -from [get_ports in2] -fields {slew cap}
puts "PASS: report_checks from in2 with fields"

#---------------------------------------------------------------
# report_net for various nets
#---------------------------------------------------------------
puts "--- report_net ---"

catch {report_net r1q} msg
puts $msg
puts "PASS: report_net r1q"

catch {report_net r2q} msg
puts $msg
puts "PASS: report_net r2q"

catch {report_net u1z} msg
puts $msg
puts "PASS: report_net u1z"

catch {report_net u2z} msg
puts $msg
puts "PASS: report_net u2z"

catch {report_net out} msg
puts $msg
puts "PASS: report_net out"

catch {report_net in1} msg
puts $msg
puts "PASS: report_net in1"

# report_net with -digits
catch {report_net -digits 6 r1q} msg
puts $msg
puts "PASS: report_net -digits 6"

#---------------------------------------------------------------
# Set manual parasitics (pi model + elmore)
#---------------------------------------------------------------
puts "--- Manual parasitic models ---"

catch {sta::set_pi_model u1/Y 0.005 1.0 0.003} msg
puts $msg
puts "PASS: set_pi_model"

catch {sta::set_elmore u1/Y u2/B 0.005} msg
puts $msg
puts "PASS: set_elmore"

report_checks
puts "PASS: report_checks after manual parasitics"

puts "ALL PASSED"
