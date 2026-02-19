# Test manual parasitic creation with coupling caps
# Targets: ConcreteParasitics.cc uncovered functions:
#   makeCapacitor (coupling cap, line 1389, hit=0)
#   ConcretePi constructor (line 138, hit=0), setPiModel (line 155, hit=0)
#   ConcretePiElmore constructor (line 182, hit=0)
#   ConcreteParasitic::setPiModel (line 92, hit=0)
#   ConcreteParasitic::setElmore (line 118, hit=0)
#   ConcreteParasitic::setIsReduced (line 105, hit=0)
#   ConcretePi::setIsReduced (line 175, hit=0)
#   ConcreteParasitics::setPiModel (line 1050, hit=0)
#   deleteParasitics(Pin) (line 824, hit=0)
#   deleteParasitics(Pin, AnalysisPt) (line 810, hit=0)
#   deleteParasitics(Net, AnalysisPt) (line 838, hit=0)
#   deleteParasiticNetworks (line 1305, hit=0)
#   deletePinBefore (line 897, hit=0)
#   makePiPoleResidue (line 1112, hit=0)
#   setPoleResidue (line 1160, hit=0)
# Also targets: Parasitics.cc, ReduceParasitics.cc
#   ReportParasiticAnnotation.cc

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
# Report before parasitics
#---------------------------------------------------------------
puts "--- before parasitics ---"
report_checks

report_parasitic_annotation

report_parasitic_annotation -report_unannotated

#---------------------------------------------------------------
# Set manual pi models on all driver pins
# set_pi_model drvr_pin c2 rpi c1
#---------------------------------------------------------------
puts "--- set_pi_model on drivers ---"

# Set pi model on u1/Y (driver of net u1z)
catch {sta::set_pi_model u1/Y 0.005 10.0 0.003} msg
puts "set_pi_model u1/Y: $msg"

# Set pi model on u2/Y (driver of net u2z)
catch {sta::set_pi_model u2/Y 0.008 15.0 0.005} msg
puts "set_pi_model u2/Y: $msg"

# Set pi model on r1/Q (driver of r1q)
catch {sta::set_pi_model r1/Q 0.002 5.0 0.001} msg
puts "set_pi_model r1/Q: $msg"

# Set pi model on r2/Q (driver of r2q)
catch {sta::set_pi_model r2/Q 0.002 5.0 0.001} msg
puts "set_pi_model r2/Q: $msg"

# Set pi model on r3/Q (driver of out)
catch {sta::set_pi_model r3/Q 0.001 2.0 0.001} msg
puts "set_pi_model r3/Q: $msg"

#---------------------------------------------------------------
# Set elmore delays on load pins
# set_elmore drvr_pin load_pin elmore
#---------------------------------------------------------------
puts "--- set_elmore on loads ---"

# Elmore delays from u1/Y to its loads
catch {sta::set_elmore u1/Y u2/A 0.005} msg
puts "set_elmore u1/Y -> u2/A: $msg"

# Elmore delays from u2/Y to its loads
catch {sta::set_elmore u2/Y r3/D 0.008} msg
puts "set_elmore u2/Y -> r3/D: $msg"

# Elmore delays from r1/Q to loads
catch {sta::set_elmore r1/Q u1/A 0.003} msg
puts "set_elmore r1/Q -> u1/A: $msg"

# Elmore delays from r2/Q to loads
catch {sta::set_elmore r2/Q u2/B 0.003} msg
puts "set_elmore r2/Q -> u2/B: $msg"

# Elmore delays from r3/Q to out port
catch {sta::set_elmore r3/Q out 0.002} msg
puts "set_elmore r3/Q -> out: $msg"

#---------------------------------------------------------------
# Check timing with manual parasitics
#---------------------------------------------------------------
puts "--- timing with manual parasitics ---"
report_checks

report_checks -path_delay min

report_checks -path_delay max

report_checks -from [get_ports in1] -to [get_ports out]

report_checks -from [get_ports in2] -to [get_ports out]

report_checks -fields {slew cap input_pins nets fanout}

#---------------------------------------------------------------
# Report parasitic annotation
#---------------------------------------------------------------
puts "--- parasitic annotation with manual ---"
report_parasitic_annotation

report_parasitic_annotation -report_unannotated

#---------------------------------------------------------------
# Report dcalc with parasitics
#---------------------------------------------------------------
puts "--- dcalc with manual parasitics ---"
report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max
puts "dcalc u1 A->Y: done"

report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max
puts "dcalc u2 A->Y: done"

report_dcalc -from [get_pins u2/B] -to [get_pins u2/Y] -max
puts "dcalc u2 B->Y: done"

report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max
puts "dcalc r1 CLK->Q: done"

report_dcalc -from [get_pins r2/CLK] -to [get_pins r2/Q] -max
puts "dcalc r2 CLK->Q: done"

report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/Q] -max
puts "dcalc r3 CLK->Q: done"

#---------------------------------------------------------------
# Report nets with different digits
#---------------------------------------------------------------
puts "--- report_net with manual parasitics ---"
foreach net_name {r1q r2q u1z u2z out} {
  report_net -digits 4 $net_name
  puts "report_net $net_name: done"
}

#---------------------------------------------------------------
# Now override with SPEF
#---------------------------------------------------------------
puts "--- override with SPEF ---"
read_spef ../../test/reg1_asap7.spef

report_checks

report_parasitic_annotation

#---------------------------------------------------------------
# Report with different dcalcs after SPEF
#---------------------------------------------------------------
puts "--- dcalc after SPEF ---"
report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max
puts "dcalc u1 after SPEF: done"

report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max
puts "dcalc u2 after SPEF: done"
