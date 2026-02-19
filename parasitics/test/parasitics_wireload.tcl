# Test parasitic estimation with pi model / elmore, deletion, and override paths.
# Targets: ConcreteParasitics.cc (makePiElmore, findElmore, deletePiElmore,
#     isPiElmore, isPiModel, piModel, setPiModel, setElmore paths,
#     delete operations, cap calculation)
# Targets: ReduceParasitics.cc (reduction with various pi models)
# Targets: Parasitics.cc (set/get/delete parasitic operations)
# Targets: EstimateParasitics.cc (estimatePiElmore tree cases)

source ../../test/helpers.tcl

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
# Baseline without parasitics
#---------------------------------------------------------------
puts "--- baseline ---"
report_checks

#---------------------------------------------------------------
# Set pi model with very small values
#---------------------------------------------------------------
puts "--- pi model very small ---"
catch {set_pi_model u1/Y 0.00001 0.01 0.000005} msg
puts "set_pi_model u1/Y tiny: $msg"

catch {set_elmore u1/Y u2/A 0.00001} msg
puts "set_elmore u1/Y->u2/A tiny: $msg"

report_checks

#---------------------------------------------------------------
# Set pi model with medium values on multiple nets
#---------------------------------------------------------------
puts "--- pi model medium ---"
catch {set_pi_model u2/Y 0.01 20.0 0.005} msg
puts "set_pi_model u2/Y medium: $msg"

catch {set_elmore u2/Y r3/D 0.01} msg
puts "set_elmore u2/Y->r3/D: $msg"

catch {set_pi_model r1/Q 0.05 50.0 0.02} msg
puts "set_pi_model r1/Q large: $msg"

catch {set_elmore r1/Q u1/A 0.05} msg
puts "set_elmore r1/Q->u1/A: $msg"

catch {set_pi_model r2/Q 0.03 30.0 0.01} msg
puts "set_pi_model r2/Q: $msg"

catch {set_elmore r2/Q u2/B 0.02} msg
puts "set_elmore r2/Q->u2/B: $msg"

report_checks

#---------------------------------------------------------------
# Different delay calculators with pi models
#---------------------------------------------------------------
puts "--- arnoldi with pi models ---"
set_delay_calculator arnoldi
report_checks

report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max
puts "arnoldi dcalc u1: done"

report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max
puts "arnoldi dcalc u2: done"

puts "--- prima with pi models ---"
set_delay_calculator prima
report_checks

puts "--- dmp_ceff_two_pole with pi models ---"
set_delay_calculator dmp_ceff_two_pole
report_checks

report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max
puts "two_pole dcalc u1: done"

report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max
puts "two_pole dcalc r1: done"

puts "--- lumped_cap with pi models ---"
set_delay_calculator lumped_cap
report_checks

report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max
puts "lumped dcalc u1: done"

#---------------------------------------------------------------
# Re-set pi model (override path)
#---------------------------------------------------------------
puts "--- override pi model ---"
set_delay_calculator dmp_ceff_elmore
catch {set_pi_model u1/Y 0.02 25.0 0.01} msg
puts "re-set u1/Y: $msg"

catch {set_pi_model u2/Y 0.005 10.0 0.002} msg
puts "re-set u2/Y: $msg"

report_checks

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

#---------------------------------------------------------------
# Now load SPEF on top (tests delete/override paths)
#---------------------------------------------------------------
puts "--- SPEF override ---"
read_spef ../../test/reg1_asap7.spef

report_checks

report_parasitic_annotation

#---------------------------------------------------------------
# Report nets after SPEF
#---------------------------------------------------------------
puts "--- report_net ---"
foreach net_name {r1q r2q u1z u2z out in1 in2} {
  report_net -digits 4 $net_name
  puts "report_net $net_name: done"
}

#---------------------------------------------------------------
# Re-read SPEF (second read exercises re-annotation)
#---------------------------------------------------------------
puts "--- re-read SPEF ---"
read_spef ../../test/reg1_asap7.spef

report_checks

report_parasitic_annotation

report_parasitic_annotation -report_unannotated
