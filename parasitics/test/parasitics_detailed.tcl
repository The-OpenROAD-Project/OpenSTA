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

#---------------------------------------------------------------
# report_parasitic_annotation
#---------------------------------------------------------------
puts "--- Parasitic annotation ---"
report_parasitic_annotation

report_parasitic_annotation -report_unannotated

#---------------------------------------------------------------
# report_annotated_delay
#---------------------------------------------------------------
puts "--- Annotated delay reporting ---"

set msg [report_annotated_delay -cell -net]
puts $msg

set msg [report_annotated_delay -from_in_ports -to_out_ports]
puts $msg

set msg [report_annotated_delay -cell]
puts $msg

set msg [report_annotated_delay -net]
puts $msg

set msg [report_annotated_delay -report_annotated]
puts $msg

set msg [report_annotated_delay -report_unannotated]
puts $msg

#---------------------------------------------------------------
# report_checks (exercises parasitics in delay calc)
#---------------------------------------------------------------
puts "--- Timing with parasitics ---"
report_checks

report_checks -path_delay min

report_checks -path_delay max

report_checks -from [get_ports in1] -fields {slew cap}

report_checks -from [get_ports in2] -fields {slew cap}

#---------------------------------------------------------------
# report_net for various nets
#---------------------------------------------------------------
puts "--- report_net ---"

set msg [report_net r1q]
puts $msg

set msg [report_net r2q]
puts $msg

set msg [report_net u1z]
puts $msg

set msg [report_net u2z]
puts $msg

set msg [report_net out]
puts $msg

set msg [report_net in1]
puts $msg

# report_net with -digits
set msg [report_net -digits 6 r1q]
puts $msg

#---------------------------------------------------------------
# Set manual parasitics (pi model + elmore)
#---------------------------------------------------------------
puts "--- Manual parasitic models ---"

set msg [sta::set_pi_model u1/Y 0.005 1.0 0.003]
puts $msg

set msg [sta::set_elmore u1/Y u2/B 0.005]
puts $msg

report_checks
