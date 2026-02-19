# Test DMP effective capacitance convergence edge cases with extreme
# slew/load conditions and manual pi model parasitics.
# Targets:
#   DmpCeff.cc: dmpCeffIter convergence paths, ceffPiElmore edge cases,
#     ceffPiPoleResidue, iteration count boundaries,
#     very small/large slew handling, zero capacitance paths
#   GraphDelayCalc.cc: findVertexDelay with manual parasitics
#   ArnoldiDelayCalc.cc: arnoldi with manual pi model

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
# Test 1: Manual pi model + dmp_ceff_elmore
# Exercises: dmpCeffIter with manually set pi model
#---------------------------------------------------------------
puts "--- Test 1: manual pi + dmp_ceff_elmore ---"

set_delay_calculator dmp_ceff_elmore

# Set pi models on all driver pins
sta::set_pi_model u1/Y 0.005 10.0 0.003
sta::set_elmore u1/Y u2/B 0.005
sta::set_pi_model u2/Y 0.008 15.0 0.005
sta::set_elmore u2/Y r3/D 0.008
sta::set_pi_model r1/Q 0.002 5.0 0.001
sta::set_elmore r1/Q u2/A 0.003
sta::set_pi_model r2/Q 0.003 6.0 0.002
sta::set_elmore r2/Q u1/A 0.004
sta::set_pi_model r3/Q 0.001 2.0 0.001
sta::set_elmore r3/Q out 0.002

report_checks

report_checks -path_delay min

#---------------------------------------------------------------
# Test 2: dmp_ceff_two_pole with manual pi
# Exercises: ceffPiPoleResidue, two-pole iteration
#---------------------------------------------------------------
puts "--- Test 2: dmp_ceff_two_pole ---"
set_delay_calculator dmp_ceff_two_pole
report_checks

report_checks -from [get_ports in1] -to [get_ports out]

report_checks -from [get_ports in2] -to [get_ports out]

#---------------------------------------------------------------
# Test 3: Extreme slew values with DMP
# Exercises: dmpCeffIter convergence boundaries
#---------------------------------------------------------------
puts "--- Test 3: extreme slew ---"

set_delay_calculator dmp_ceff_elmore

# Very small slew
set_input_transition 0.01 {in1 in2 clk1 clk2 clk3}
report_checks

# Small slew
set_input_transition 0.1 {in1 in2 clk1 clk2 clk3}
report_checks

# Medium slew
set_input_transition 50 {in1 in2 clk1 clk2 clk3}
report_checks

# Large slew
set_input_transition 500 {in1 in2 clk1 clk2 clk3}
report_checks

# Very large slew
set_input_transition 2000 {in1 in2 clk1 clk2 clk3}
report_checks

set_input_transition 10 {in1 in2 clk1 clk2 clk3}

#---------------------------------------------------------------
# Test 4: Very small pi model values (near-zero)
# Exercises: DMP edge case with tiny parasitics
#---------------------------------------------------------------
puts "--- Test 4: tiny pi model ---"

sta::set_pi_model u1/Y 0.00001 0.1 0.00001
sta::set_elmore u1/Y u2/B 0.00001
report_checks

#---------------------------------------------------------------
# Test 5: Large pi model values
# Exercises: DMP with heavy parasitic loading
#---------------------------------------------------------------
puts "--- Test 5: large pi model ---"

sta::set_pi_model u1/Y 0.1 100.0 0.05
sta::set_elmore u1/Y u2/B 0.5
sta::set_pi_model u2/Y 0.15 150.0 0.08
sta::set_elmore u2/Y r3/D 0.8
report_checks

#---------------------------------------------------------------
# Test 6: report_dcalc with dmp calculators
# Exercises: DMP ceff iteration for specific arcs
#---------------------------------------------------------------
puts "--- Test 6: report_dcalc ---"

set_delay_calculator dmp_ceff_elmore
report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max -digits 6
puts "dmp_elmore u1: done"

report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max -digits 4
puts "dmp_elmore u2 A: done"

report_dcalc -from [get_pins u2/B] -to [get_pins u2/Y] -max
puts "dmp_elmore u2 B: done"

report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max
puts "dmp_elmore r1: done"

report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/Q] -min
puts "dmp_elmore r3 min: done"

set_delay_calculator dmp_ceff_two_pole
report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max -digits 6
puts "dmp_two_pole u1: done"

report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max
puts "dmp_two_pole u2: done"

report_dcalc -from [get_pins r2/CLK] -to [get_pins r2/Q] -max
puts "dmp_two_pole r2: done"

#---------------------------------------------------------------
# Test 7: SPEF overriding manual, then DMP
# Exercises: deleteReducedParasitics from manual->SPEF transition
#---------------------------------------------------------------
puts "--- Test 7: SPEF override manual ---"
set_delay_calculator dmp_ceff_elmore
read_spef ../../test/reg1_asap7.spef

report_checks

set_delay_calculator dmp_ceff_two_pole
report_checks

#---------------------------------------------------------------
# Test 8: Load variation with DMP
# Exercises: loadPinCapacitanceChanged -> deleteReducedParasitics
#---------------------------------------------------------------
puts "--- Test 8: load variation ---"
set_delay_calculator dmp_ceff_elmore

foreach load_val {0.0001 0.001 0.01 0.05 0.1} {
  set_load $load_val [get_ports out]
  report_checks
  puts "dmp load=$load_val: done"
}
set_load 0 [get_ports out]

#---------------------------------------------------------------
# Test 9: find_delays and invalidation
#---------------------------------------------------------------
puts "--- Test 9: find_delays ---"
sta::find_delays

sta::delays_invalid
sta::find_delays
