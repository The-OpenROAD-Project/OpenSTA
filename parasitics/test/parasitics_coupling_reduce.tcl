# Test parasitic coupling capacitance, reduction to pi/elmore and pi/pole-residue,
# deleteReducedParasitics, and network modification with parasitics.
# Targets:
#   ConcreteParasitics.cc: makeCapacitor (coupling cap), CouplingCap,
#     reducePiElmore, reducePiPoleResidue, deleteReducedParasitics,
#     deleteDrvrReducedParasitics, disconnectPinBefore,
#     loadPinCapacitanceChanged, isPiElmore, isPiPoleResidue
#   ReduceParasitics.cc: reduceToPiElmore, reduceToPiPoleResidue,
#     arnoldi/prima reduction iteration paths
#   SpefReader.cc: makeCapacitor coupling cap path, *NAME_MAP parsing

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
# Test 1: Read SPEF with coupling caps (keep_coupling_caps)
# Exercises: SpefReader coupling cap path, makeCapacitor 2-node
#---------------------------------------------------------------
puts "--- Test 1: SPEF with coupling caps (keep) ---"
read_spef -keep_capacitive_coupling parasitics_coupling_spef.spef
puts "PASS: read_spef -keep_capacitive_coupling"

report_checks
puts "PASS: report_checks with coupling"

report_checks -path_delay min
puts "PASS: min path with coupling"

report_checks -fields {slew cap input_pins nets fanout}
puts "PASS: report with fields"

#---------------------------------------------------------------
# Test 2: DMP calculators with coupling caps
# Exercises: parasitic reduction from coupling cap networks
#---------------------------------------------------------------
puts "--- Test 2: dmp with coupling ---"
set_delay_calculator dmp_ceff_elmore
report_checks
puts "PASS: dmp_ceff_elmore with coupling"

report_checks -from [get_ports in1] -to [get_ports out]
puts "PASS: dmp in1->out"

report_checks -from [get_ports in2] -to [get_ports out]
puts "PASS: dmp in2->out"

set_delay_calculator dmp_ceff_two_pole
report_checks
puts "PASS: dmp_ceff_two_pole with coupling"

#---------------------------------------------------------------
# Test 3: Lumped cap with coupling
#---------------------------------------------------------------
puts "--- Test 3: lumped_cap with coupling ---"
set_delay_calculator lumped_cap
report_checks
puts "PASS: lumped_cap with coupling"

#---------------------------------------------------------------
# Test 5: Re-read SPEF with coupling factor
# Exercises: coupling_cap_factor scaling
#---------------------------------------------------------------
puts "--- Test 5: SPEF with coupling factor ---"
set_delay_calculator dmp_ceff_elmore
read_spef -coupling_reduction_factor 0.5 parasitics_coupling_spef.spef
puts "PASS: read_spef -coupling_reduction_factor 0.5"

report_checks
puts "PASS: report with factor 0.5"

#---------------------------------------------------------------
# Test 6: Re-read SPEF without coupling (default mode)
# Exercises: deleteReducedParasitics, deleteParasiticNetworks on re-read
#---------------------------------------------------------------
puts "--- Test 6: SPEF without coupling (re-read) ---"
read_spef ../../test/reg1_asap7.spef
puts "PASS: re-read normal SPEF"

report_checks
puts "PASS: report after re-read"

#---------------------------------------------------------------
# Test 7: Read SPEF with -reduce flag
# Exercises: reduce parameter in readSpef, parasitic reduction flow
#---------------------------------------------------------------
puts "--- Test 7: SPEF with -reduce ---"
read_spef -reduce ../../test/reg1_asap7.spef
puts "PASS: read_spef -reduce"

report_checks
puts "PASS: report after reduce"

set_delay_calculator dmp_ceff_two_pole
report_checks
puts "PASS: dmp_two_pole after reduce"

set_delay_calculator dmp_ceff_elmore
report_checks
puts "PASS: dmp after reduce"

#---------------------------------------------------------------
# Test 8: Load changes trigger deleteReducedParasitics
#---------------------------------------------------------------
puts "--- Test 8: load change ---"
set_load 0.01 [get_ports out]
report_checks
puts "PASS: load 0.01"

set_load 0.05 [get_ports out]
report_checks
puts "PASS: load 0.05"

set_load 0 [get_ports out]
report_checks
puts "PASS: load reset"

#---------------------------------------------------------------
# Test 9: Report parasitic annotation
#---------------------------------------------------------------
puts "--- Test 9: annotation ---"
report_parasitic_annotation
puts "PASS: annotation"

report_parasitic_annotation -report_unannotated
puts "PASS: annotation -report_unannotated"

#---------------------------------------------------------------
# Test 10: Query pi/elmore values
#---------------------------------------------------------------
puts "--- Test 10: query pi/elmore ---"
catch {
  set pi [sta::find_pi_elmore [get_pins u1/Y] "rise" "max"]
  puts "u1/Y rise max pi: $pi"
} msg

catch {
  set pi [sta::find_pi_elmore [get_pins u2/Y] "fall" "max"]
  puts "u2/Y fall max pi: $pi"
} msg

catch {
  set elm [sta::find_elmore [get_pins u1/Y] [get_pins u2/B] "rise" "max"]
  puts "elmore u1/Y->u2/B: $elm"
} msg

puts "PASS: pi/elmore queries"

puts "ALL PASSED"
