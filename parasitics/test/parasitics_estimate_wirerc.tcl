# Test parasitic estimation with set_wire_rc and wireload models
# Targets: EstimateParasitics.cc (estimatePiElmore, wireload-based estimation)
#   ConcreteParasitics.cc (makeParasiticPi, makeParasiticPiElmore, setPiModel,
#     setElmore, findPiElmore, findElmore, delete operations)
#   ReduceParasitics.cc (parasitic reduction with estimated parasitics)
#   Parasitics.cc (estimate operations, delete parasitics, find parasitics)
#   SpefReader.cc (additional format coverage)

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
# Baseline without any parasitics
#---------------------------------------------------------------
puts "--- baseline ---"
report_checks

report_parasitic_annotation

#---------------------------------------------------------------
# Set pi model on multiple nets, with varying parameter values
#---------------------------------------------------------------
puts "--- set_pi_model with varied parameters ---"

# Very small parasitics
catch {sta::set_pi_model u1/Y 0.0001 0.1 0.00005} msg
puts "set_pi_model u1/Y (small): $msg"

# Medium parasitics
catch {sta::set_pi_model u2/Y 0.01 20.0 0.005} msg
puts "set_pi_model u2/Y (medium): $msg"

# Large parasitics
catch {sta::set_pi_model r1/Q 0.05 50.0 0.02} msg
puts "set_pi_model r1/Q (large): $msg"

catch {sta::set_pi_model r2/Q 0.03 30.0 0.01} msg
puts "set_pi_model r2/Q: $msg"

catch {sta::set_pi_model r3/Q 0.001 2.0 0.001} msg
puts "set_pi_model r3/Q: $msg"

#---------------------------------------------------------------
# Set elmore delays with different values
#---------------------------------------------------------------
puts "--- set_elmore varied ---"

catch {sta::set_elmore u1/Y u2/A 0.0001} msg
puts "set_elmore u1/Y -> u2/A (small): $msg"

catch {sta::set_elmore u2/Y r3/D 0.01} msg
puts "set_elmore u2/Y -> r3/D (medium): $msg"

catch {sta::set_elmore r1/Q u1/A 0.05} msg
puts "set_elmore r1/Q -> u1/A (large): $msg"

catch {sta::set_elmore r2/Q u2/B 0.02} msg
puts "set_elmore r2/Q -> u2/B: $msg"

catch {sta::set_elmore r3/Q out 0.001} msg
puts "set_elmore r3/Q -> out: $msg"

#---------------------------------------------------------------
# Report with estimated parasitics
#---------------------------------------------------------------
puts "--- timing with varied parasitics ---"
report_checks

report_checks -path_delay min

report_checks -path_delay max

report_checks -fields {slew cap input_pins nets fanout}

#---------------------------------------------------------------
# Try different delay calculators with these parasitics
#---------------------------------------------------------------
puts "--- arnoldi with estimated ---"
set_delay_calculator arnoldi
report_checks

report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max
puts "arnoldi dcalc u1: done"

report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max
puts "arnoldi dcalc u2: done"

puts "--- lumped_cap with estimated ---"
set_delay_calculator lumped_cap
report_checks

report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max
puts "lumped_cap dcalc u1: done"

puts "--- dmp_ceff_two_pole with estimated ---"
set_delay_calculator dmp_ceff_two_pole
report_checks

report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max
puts "dmp_ceff_two_pole dcalc r1: done"

#---------------------------------------------------------------
# Override pi model (re-set on same pin)
#---------------------------------------------------------------
puts "--- override pi model ---"
set_delay_calculator dmp_ceff_elmore
catch {sta::set_pi_model u1/Y 0.02 25.0 0.01} msg
puts "re-set pi_model u1/Y: $msg"

report_checks

#---------------------------------------------------------------
# Now load SPEF on top to override manual (tests delete/override paths)
#---------------------------------------------------------------
puts "--- SPEF override ---"
read_spef ../../test/reg1_asap7.spef

report_checks

report_parasitic_annotation

report_parasitic_annotation -report_unannotated

#---------------------------------------------------------------
# Report nets with parasitics
#---------------------------------------------------------------
puts "--- report_net after SPEF ---"
foreach net_name {r1q r2q u1z u2z out in1 in2} {
  report_net -digits 4 $net_name
  puts "report_net $net_name: done"
}

#---------------------------------------------------------------
# Annotated delay reporting
#---------------------------------------------------------------
puts "--- annotated delay ---"
report_annotated_delay -cell -net
puts "annotated -cell -net: done"

report_annotated_delay -from_in_ports -to_out_ports
puts "annotated from/to: done"

report_annotated_delay -report_annotated
puts "annotated -report_annotated: done"

report_annotated_delay -report_unannotated
puts "annotated -report_unannotated: done"
