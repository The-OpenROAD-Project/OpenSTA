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

report_checks

report_checks -path_delay min

report_checks -fields {slew cap input_pins nets fanout}

#---------------------------------------------------------------
# Test 2: DMP calculators with coupling caps
# Exercises: parasitic reduction from coupling cap networks
#---------------------------------------------------------------
puts "--- Test 2: dmp with coupling ---"
set_delay_calculator dmp_ceff_elmore
report_checks

report_checks -from [get_ports in1] -to [get_ports out]

report_checks -from [get_ports in2] -to [get_ports out]

set_delay_calculator dmp_ceff_two_pole
report_checks

#---------------------------------------------------------------
# Test 3: Lumped cap with coupling
#---------------------------------------------------------------
puts "--- Test 3: lumped_cap with coupling ---"
set_delay_calculator lumped_cap
report_checks

#---------------------------------------------------------------
# Test 5: Re-read SPEF with coupling factor
# Exercises: coupling_cap_factor scaling
#---------------------------------------------------------------
puts "--- Test 5: SPEF with coupling factor ---"
set_delay_calculator dmp_ceff_elmore
read_spef -coupling_reduction_factor 0.5 parasitics_coupling_spef.spef

report_checks

#---------------------------------------------------------------
# Test 6: Re-read SPEF without coupling (default mode)
# Exercises: deleteReducedParasitics, deleteParasiticNetworks on re-read
#---------------------------------------------------------------
puts "--- Test 6: SPEF without coupling (re-read) ---"
read_spef ../../test/reg1_asap7.spef

report_checks

#---------------------------------------------------------------
# Test 7: Read SPEF with -reduce flag
# Exercises: reduce parameter in readSpef, parasitic reduction flow
#---------------------------------------------------------------
puts "--- Test 7: SPEF with -reduce ---"
read_spef -reduce ../../test/reg1_asap7.spef

report_checks

set_delay_calculator dmp_ceff_two_pole
report_checks

set_delay_calculator dmp_ceff_elmore
report_checks

#---------------------------------------------------------------
# Test 8: Load changes trigger deleteReducedParasitics
#---------------------------------------------------------------
puts "--- Test 8: load change ---"
set_load 0.01 [get_ports out]
report_checks

set_load 0.05 [get_ports out]
report_checks

set_load 0 [get_ports out]
report_checks

#---------------------------------------------------------------
# Test 9: Report parasitic annotation
#---------------------------------------------------------------
puts "--- Test 9: annotation ---"
report_parasitic_annotation

report_parasitic_annotation -report_unannotated

#---------------------------------------------------------------
# Test 10: Query pi/elmore values
#---------------------------------------------------------------
puts "--- Test 10: query pi/elmore ---"
set pi [sta::find_pi_elmore [get_pins u1/Y] "rise" "max"]
puts "u1/Y rise max pi: $pi"

set pi [sta::find_pi_elmore [get_pins u2/Y] "fall" "max"]
puts "u2/Y fall max pi: $pi"

set elm [sta::find_elmore [get_pins u1/Y] [get_pins u2/B] "rise" "max"]
puts "elmore u1/Y->u2/B: $elm"
